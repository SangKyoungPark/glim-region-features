# main.py - GlimRegion 로컬 분석 대시보드 (FastAPI)
#
# 브라우저 → 이 서버 → GlimRegionBatch.exe 실행 → CSV+오버레이 파싱 → JSON.
# C++ 계산 엔진 재사용, 웹은 표시만. (같은 PC 전제, 폴더는 서버 로컬 경로 텍스트 입력)
#
# 실행: web/run.bat  또는  python -m uvicorn main:app --host 127.0.0.1 --port 8080
import os
import csv
import re
import time
import hashlib
import subprocess
from collections import defaultdict, OrderedDict

from fastapi import FastAPI, Query
from fastapi.responses import JSONResponse, FileResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

import config

app = FastAPI(title="GlimRegion Dashboard")

STATIC_DIR = os.path.join(config.BASE_DIR, "static")
os.makedirs(STATIC_DIR, exist_ok=True)
os.makedirs(config.CACHE_DIR, exist_ok=True)
app.mount("/static", StaticFiles(directory=STATIC_DIR), name="static")

# /api/image 경로 방어용: 실행했던 입력 폴더(정규화 절대경로) 집합
ALLOWED_DIRS = set()


def _norm(p):
    return os.path.normcase(os.path.normpath(os.path.abspath(p)))


def _is_allowed_image_path(path):
    """캐시 폴더 내부 또는 실행했던 입력 폴더 내부만 허용(path traversal 방어)."""
    np = _norm(path)
    cache = _norm(config.CACHE_DIR)
    if np.startswith(cache + os.sep) or np == cache:
        return True
    for d in ALLOWED_DIRS:
        if np.startswith(d + os.sep) or np == d:
            return True
    return False


class RunRequest(BaseModel):
    folderPath: str
    profile: str = "none"        # coater | rollpress | slitter | none
    binarize: str = "bright127"  # bright127 | dark127 | darkauto | brightauto
    threads: int = 0             # 0 = 자동


def _to_float(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def _parse_csv(csv_path, overlay_dir):
    """CSV → (rows, columns, score_features)."""
    rows = []
    columns = []
    score_features = []
    with open(csv_path, "r", newline="", encoding="utf-8", errors="replace") as f:
        reader = csv.DictReader(f)
        columns = reader.fieldnames or []
        score_features = [c[len("score_"):] for c in columns if c.startswith("score_")]
        for rec in reader:
            region_index = rec.get("RegionIndex", "")
            try:
                ri = int(float(region_index))
            except (TypeError, ValueError):
                ri = -1

            file_name = rec.get("FileName", "")
            file_path = rec.get("FilePath", "")
            overlay_path = os.path.join(overlay_dir, file_name + "_ov.png") if file_name else ""

            scores = {}
            for feat in score_features:
                v = _to_float(rec.get("score_" + feat, ""))
                if v is not None:
                    scores[feat] = v

            # 전체 원시 특징값(비어있지 않은 값만) — 상세 패널용
            raw = {}
            for c in columns:
                val = rec.get(c, "")
                if val is None or val == "":
                    continue
                fv = _to_float(val)
                raw[c] = fv if fv is not None else val

            rows.append({
                "fileName": file_name,
                "filePath": file_path,
                "overlayPath": overlay_path,
                "regionIndex": ri,
                "area": _to_float(rec.get("area", "")),
                "circularity": _to_float(rec.get("circularity", "")),
                "roundness": _to_float(rec.get("roundness", "")),
                "anisometry": _to_float(rec.get("anisometry", "")),
                "convexity": _to_float(rec.get("convexity", "")),
                "contlength": _to_float(rec.get("contlength", "")),
                "diameter": _to_float(rec.get("diameter", "")),
                "classifiedCode": rec.get("ClassifiedCode") if "ClassifiedCode" in columns else None,
                "scores": scores,
                "raw": raw,
            })
    return rows, columns, score_features


def _summarize(rows, score_features):
    total_regions = sum(1 for r in rows if r["regionIndex"] >= 0)
    no_region = sum(1 for r in rows if r["regionIndex"] < 0)
    image_names = set(r["fileName"] for r in rows)

    # 불량 분포(ClassifiedCode 별 카운트) — Region 행만
    dist = defaultdict(int)
    for r in rows:
        if r["regionIndex"] < 0:
            code = "NO_REGION"
        else:
            code = r.get("classifiedCode") or "(none)"
        dist[code] += 1
    dist_list = [{"code": k, "count": v} for k, v in
                 sorted(dist.items(), key=lambda kv: (-kv[1], kv[0]))]

    # 분류코드별 score_* 평균
    acc = defaultdict(lambda: defaultdict(lambda: [0.0, 0]))  # code -> feat -> [sum,count]
    for r in rows:
        if r["regionIndex"] < 0:
            continue
        code = r.get("classifiedCode") or "(none)"
        for feat, v in r["scores"].items():
            acc[code][feat][0] += v
            acc[code][feat][1] += 1
    score_by_code = {}
    for code, feats in acc.items():
        score_by_code[code] = {
            feat: (s / c if c else 0.0) for feat, (s, c) in feats.items()
        }

    return {
        "summary": {
            "totalImages": len(image_names),
            "totalRegions": total_regions,
            "noRegionCount": no_region,
        },
        "defectDistribution": dist_list,
        "scoreByCode": score_by_code,
    }


@app.get("/", response_class=HTMLResponse)
def index():
    idx = os.path.join(STATIC_DIR, "index.html")
    if not os.path.isfile(idx):
        return HTMLResponse("<h1>GlimRegion Dashboard</h1><p>static/index.html 준비 중</p>")
    with open(idx, "r", encoding="utf-8") as f:
        return HTMLResponse(f.read())


@app.post("/api/run")
def api_run(req: RunRequest):
    folder = req.folderPath.strip().strip('"')
    if not folder or not os.path.isdir(folder):
        return JSONResponse({"ok": False, "error": f"폴더를 찾을 수 없습니다: {folder}"}, status_code=400)

    exe = config.find_batch_exe()
    if not exe:
        return JSONResponse({"ok": False, "error": "GlimRegionBatch.exe 를 찾을 수 없습니다. 먼저 Release 빌드가 필요합니다."}, status_code=500)

    profile_key = (req.profile or "none").lower()
    if profile_key not in config.PROFILE_MAP:
        profile_key = "none"
    binarize_key = (req.binarize or "bright127").lower()
    if binarize_key not in config.BINARIZE_MAP:
        binarize_key = "bright127"

    # 실행 캐시 폴더 = 해시(폴더+프로파일+이진화)
    key = "%s|%s|%s" % (_norm(folder), profile_key, binarize_key)
    h = hashlib.md5(key.encode("utf-8")).hexdigest()[:16]
    out_dir = os.path.join(config.CACHE_DIR, h)
    overlay_dir = os.path.join(out_dir, "overlay")
    os.makedirs(overlay_dir, exist_ok=True)
    csv_path = os.path.join(out_dir, "result.csv")

    # exe 인자 구성
    args = [exe, folder, csv_path]
    profile_ini = config.PROFILE_MAP.get(profile_key)
    if profile_ini:
        args.append(os.path.join(config.PROFILES_DIR, profile_ini))
    args += config.BINARIZE_MAP[binarize_key]
    if req.threads and req.threads > 0:
        args += ["--threads", str(int(req.threads))]
    args += ["--overlay", overlay_dir]

    # OpenCV DLL 을 PATH 에 추가하고 실행
    env = dict(os.environ)
    env["PATH"] = config.OPENCV_BIN + os.pathsep + env.get("PATH", "")

    t0 = time.time()
    try:
        proc = subprocess.run(args, env=env, capture_output=True, text=True, timeout=600)
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"exe 실행 실패: {e}"}, status_code=500)
    elapsed_ms = int((time.time() - t0) * 1000)

    if not os.path.isfile(csv_path):
        return JSONResponse({
            "ok": False,
            "error": "CSV 결과가 생성되지 않았습니다.",
            "stdout": proc.stdout, "stderr": proc.stderr,
        }, status_code=500)

    # exe stdout 의 elapsed(ms) 파싱(있으면 우선)
    m = re.search(r"elapsed \(ms\)\s*:\s*(\d+)", proc.stdout or "")
    exe_elapsed = int(m.group(1)) if m else None

    rows, columns, score_features = _parse_csv(csv_path, overlay_dir)
    agg = _summarize(rows, score_features)

    # 이미지 서빙 허용 폴더 등록
    ALLOWED_DIRS.add(_norm(folder))

    return {
        "ok": True,
        "hash": h,
        "profile": profile_key,
        "binarize": binarize_key,
        "columns": columns,
        "scoreFeatures": score_features,
        "summary": dict(agg["summary"], elapsedMs=exe_elapsed if exe_elapsed is not None else elapsed_ms),
        "defectDistribution": agg["defectDistribution"],
        "scoreByCode": agg["scoreByCode"],
        "rows": rows,
    }


@app.get("/api/image")
def api_image(path: str = Query(...), overlay: int = 0):
    if not _is_allowed_image_path(path):
        return JSONResponse({"error": "허용되지 않은 경로"}, status_code=403)
    if not os.path.isfile(path):
        return JSONResponse({"error": "파일 없음"}, status_code=404)
    return FileResponse(path)


if __name__ == "__main__":
    import uvicorn
    os.makedirs(config.CACHE_DIR, exist_ok=True)
    uvicorn.run(app, host=config.HOST, port=config.PORT)
