# main.py - GlimRegion 로컬 분석 대시보드 (FastAPI)
#
# 브라우저 → 이 서버 → GlimRegionBatch.exe 실행 → CSV+오버레이 파싱 → JSON.
# C++ 계산 엔진 재사용, 웹은 표시만. (같은 PC 전제, 폴더는 서버 로컬 경로 텍스트 입력)
#
# 실행: web/run.bat  또는  python -m uvicorn main:app --host 127.0.0.1 --port 8080
import os
import csv
import re
import json
import time
import hashlib
import subprocess
import threading
from collections import defaultdict, OrderedDict

from fastapi import FastAPI, Query
from fastapi.responses import JSONResponse, FileResponse, HTMLResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

import config
import db  # 검사 결과 PostgreSQL 저장(설정 없으면 no-op)

app = FastAPI(title="GlimRegion Dashboard")


@app.on_event("startup")
def _init_db():
    # DB 설정(db_config.json / GRF_PG_DSN)이 있으면 스키마 보장. 없으면 조용히 건너뜀.
    try:
        if db.is_enabled():
            ok = db.ensure_schema()
            print("[db] enabled, schema ready" if ok else "[db] enabled but schema init failed")
        else:
            print("[db] disabled (db_config.json / GRF_PG_DSN 없음) — 저장 없이 동작")
    except Exception as e:
        print(f"[db] init error: {e}")

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
    threads: int = 0             # 0 = 자동
    # 이진화(granular). mode 비면 legacy binarize 키로 폴백.
    polarity: str = "bright"     # bright | dark
    mode: str = ""               # fixed | auto | binary | wrinkle | projection | ""(legacy)
    thresh: int = 127            # fixed 용 (0~255)
    offset: int = 20             # auto 용
    kernel: int = 15             # wrinkle 용 (BLACKHAT 수평 RECT 커널 폭)
    blur: int = 31               # wrinkle 용 (세로 누적 blur 길이)
    response: int = 4            # wrinkle 용 (누적 응답 임계값)
    blackTh: int = 20            # projection 용 (흑=어두움 임계값)
    whiteTh: int = 235           # projection 용 (백=밝음 임계값)
    projKernel: int = 3          # projection 용 (모폴로지 커널, 0=미사용)
    scaleX: float = 1.0          # 픽셀→mm 환산 X (공통, 1.0=미환산)
    scaleY: float = 1.0          # 픽셀→mm 환산 Y (공통, 1.0=미환산)
    binarize: str = "bright127"  # legacy: bright127 | dark127 | darkauto | brightauto | binary


class ClusterRequest(BaseModel):
    # 이진화/폴더 파라미터는 RunRequest 와 동일 형태(설정 탭 값 재사용).
    folderPath: str
    threads: int = 0
    polarity: str = "bright"     # bright | dark
    mode: str = ""               # fixed | auto | binary | wrinkle | projection | ""(legacy)
    thresh: int = 127
    offset: int = 20
    kernel: int = 15
    blur: int = 31
    response: int = 4
    blackTh: int = 20
    whiteTh: int = 235
    projKernel: int = 3
    binarize: str = "bright127"
    # 군집 전용 파라미터
    features: str = "shape8"     # 쉼표 문자열 또는 "shape8" / "all" 프리셋
    scaleMode: str = "zscore"    # zscore | minmax | robust
    k: int = 0                   # 0 = auto
    kMax: int = 8


# ===================== 설정 프리셋 (web/presets.json 영속화) =====================
# 프리셋 = {name, settings{...}, builtin?}. settings 는 프론트 readSettings() 형태 그대로.
# 파일 동시 접근 방어용 락(단일 프로세스 다중 요청 대비).
_PRESETS_LOCK = threading.Lock()

# 기본 제공 프리셋 2종.
#  근거: D:\128Crop 실측 60장 회귀 검증 결과(직전 web-projection 통합 보고서).
#  - 흑점(Projection): 검사기 동일 이진화. mode=projection, blackTh=20 에서 B채널 4 Region 재현.
#  - 무지부 주름(Wrinkle): BLACKHAT 수평 커널 기본값(kernel15/blur31/response4)에서 주름선 검출.
DEFAULT_PRESETS = [
    {
        "name": "흑점(Projection)",
        "builtin": True,
        "settings": {
            "folderPath": "", "profile": "none", "threads": 0,
            "polarity": "dark", "mode": "projection",
            "thresh": 127, "offset": 20, "kernel": 15, "blur": 31, "response": 4,
            "blackTh": 20, "whiteTh": 235, "projKernel": 3,
            "scaleX": 1.0, "scaleY": 1.0,
        },
    },
    {
        "name": "무지부 주름(Wrinkle)",
        "builtin": True,
        "settings": {
            "folderPath": "", "profile": "none", "threads": 0,
            "polarity": "dark", "mode": "wrinkle",
            "thresh": 127, "offset": 20, "kernel": 15, "blur": 31, "response": 4,
            "blackTh": 20, "whiteTh": 235, "projKernel": 3,
            "scaleX": 1.0, "scaleY": 1.0,
        },
    },
]

# 프리셋 settings 로 허용하는 키(화이트리스트) — 임의 키 저장 방지.
_PRESET_KEYS = {
    "folderPath", "profile", "threads", "polarity", "mode",
    "thresh", "offset", "kernel", "blur", "response",
    "blackTh", "whiteTh", "projKernel", "scaleX", "scaleY", "binarize",
}


def _sanitize_preset_settings(s):
    """dict 에서 허용 키만 추출(방어). 값 타입은 프론트/exe 쪽에서 재검증하므로 그대로 보존."""
    if not isinstance(s, dict):
        return {}
    return {k: v for k, v in s.items() if k in _PRESET_KEYS}


def _load_presets_unlocked():
    """락 보유 상태에서 presets.json 로드. 없으면 기본 2종 생성. 항상 list 반환."""
    if not os.path.isfile(config.PRESETS_FILE):
        _write_presets_unlocked(DEFAULT_PRESETS)
        return [dict(p) for p in DEFAULT_PRESETS]
    try:
        with open(config.PRESETS_FILE, "r", encoding="utf-8") as f:
            data = json.load(f)
        presets = data.get("presets", []) if isinstance(data, dict) else data
        if not isinstance(presets, list):
            presets = []
        return presets
    except Exception:
        # 손상 시 기본값으로 폴백(덮어쓰지는 않음 — 사용자 데이터 보존 우선)
        return [dict(p) for p in DEFAULT_PRESETS]


def _load_presets():
    """presets.json 로드(락 획득). 없으면 기본 2종으로 생성. 항상 list 반환."""
    with _PRESETS_LOCK:
        return _load_presets_unlocked()


def _write_presets_unlocked(presets):
    """락 보유 상태에서 원자적 쓰기(임시파일→교체)."""
    tmp = config.PRESETS_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump({"presets": presets}, f, ensure_ascii=False, indent=2)
    os.replace(tmp, config.PRESETS_FILE)


def _save_presets(presets):
    with _PRESETS_LOCK:
        _write_presets_unlocked(presets)


class PresetSaveRequest(BaseModel):
    name: str
    settings: dict


# 이미지 확장자
IMG_EXTS = (".bmp", ".png", ".jpg", ".jpeg", ".tif", ".tiff")


def _list_images(folder, n=None):
    try:
        names = sorted(f for f in os.listdir(folder)
                       if os.path.isfile(os.path.join(folder, f))
                       and os.path.splitext(f)[1].lower() in IMG_EXTS)
    except OSError:
        return []
    return names[:n] if n else names


# exe usage 문자열 캐시 → 신규 플래그(--dumpbin/--scale-x 등) 지원 여부 판별.
# 엔진 병행 작업이 아직 미반영이면 해당 플래그는 전달하지 않고 무해하게 폴백한다.
_EXE_CAPS = {}


def exe_supports(exe, flag):
    """exe 인자 없이 실행 → usage 출력에 flag 문자열이 있으면 지원으로 간주(결과 캐시)."""
    caps = _EXE_CAPS.get(exe)
    if caps is None:
        caps = ""
        try:
            env = dict(os.environ)
            env["PATH"] = config.OPENCV_BIN + os.pathsep + env.get("PATH", "")
            p = subprocess.run([exe], env=env, capture_output=True, text=True, timeout=15)
            caps = (p.stdout or "") + (p.stderr or "")
        except Exception:
            caps = ""
        _EXE_CAPS[exe] = caps
    return flag in caps


def build_binarize(polarity, mode, thresh, offset, legacy_key=None, kernel=15, response=4, blur=31,
                   black_th=20, white_th=235, proj_kernel=3):
    """granular 파라미터 → GlimRegionBatch CLI 플래그 + 캐시 라벨. mode 비면 legacy 키 폴백."""
    polarity = (polarity or "").lower()
    mode = (mode or "").lower()
    try:
        thresh = int(thresh)
    except (TypeError, ValueError):
        thresh = 127
    try:
        offset = int(offset)
    except (TypeError, ValueError):
        offset = 20
    try:
        kernel = int(kernel)
    except (TypeError, ValueError):
        kernel = 15
    try:
        response = int(response)
    except (TypeError, ValueError):
        response = 4
    try:
        blur = int(blur)
    except (TypeError, ValueError):
        blur = 31
    try:
        black_th = int(black_th)
    except (TypeError, ValueError):
        black_th = 20
    try:
        white_th = int(white_th)
    except (TypeError, ValueError):
        white_th = 235
    try:
        proj_kernel = int(proj_kernel)
    except (TypeError, ValueError):
        proj_kernel = 3

    if mode not in ("fixed", "auto", "binary", "wrinkle", "projection"):
        lk = (legacy_key or "bright127").lower()
        if lk == "dark127":
            polarity, mode, thresh = "dark", "fixed", 127
        elif lk == "darkauto":
            polarity, mode = "dark", "auto"
        elif lk == "brightauto":
            polarity, mode = "bright", "auto"
        elif lk == "binary":
            polarity, mode = (polarity or "bright"), "binary"
        else:  # bright127
            polarity, mode, thresh = "bright", "fixed", 127

    if polarity not in ("bright", "dark"):
        polarity = "bright"

    # PROJECTION 은 극성과 무관(흑/백 2채널 동시 산출). 검사기 동일 이진화.
    if mode == "projection":
        flags = ["--proj", "--black-th", str(black_th), "--white-th", str(white_th)]
        label = "proj-b%d-w%d" % (black_th, white_th)
        if proj_kernel and proj_kernel > 0:
            flags += ["--proj-kernel", str(proj_kernel)]
            label += "-k%d" % proj_kernel
        return flags, label

    # WRINKLE 은 극성과 무관(항상 어두운 선 검출). 별도 플래그 세트로 구성.
    if mode == "wrinkle":
        kernel = max(1, kernel)
        blur = max(1, blur)
        flags = ["--wrinkle", "--kernel", str(kernel),
                 "--blur", str(blur), "--response", str(response)]
        label = "wrinkle-k%d-b%d-r%d" % (kernel, blur, response)
        return flags, label

    flags = ["--dark"] if polarity == "dark" else ["--bright"]
    if mode == "binary":
        flags += ["--binary"]
        label = "%s-binary" % polarity
    elif mode == "auto":
        flags += ["--thresh", "auto", "--offset", str(offset)]
        label = "%s-auto-off%d" % (polarity, offset)
    else:  # fixed
        thresh = max(0, min(255, thresh))
        flags += ["--thresh", str(thresh)]
        label = "%s-fixed-th%d" % (polarity, thresh)
    return flags, label


def _to_float(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return None


def _bin_png_path(bin_dir, file_name, channel):
    """--dumpbin 결과 이진화 PNG 경로 후보 중 실제 존재하는 것 반환(없으면 "").
    projection: <이름>_bin_B.png / _bin_W.png, 그 외 단일: <이름>_bin.png.
    엔진 명명 편차 대비로 fileName(확장자 포함)·stem 두 후보를 모두 확인한다."""
    if not bin_dir or not file_name:
        return ""
    stem = os.path.splitext(file_name)[0]
    suffixes = []
    ch = (channel or "").upper()
    if ch in ("B", "W"):
        suffixes += ["_bin_%s.png" % ch]
    suffixes += ["_bin.png"]
    for base in (file_name, stem):
        for suf in suffixes:
            cand = os.path.join(bin_dir, base + suf)
            if os.path.isfile(cand):
                return cand
    return ""


def _parse_csv(csv_path, overlay_dir, bin_dir=None):
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
            channel = (rec.get("Channel", "") or "").strip()
            overlay_path = os.path.join(overlay_dir, file_name + "_ov.png") if file_name else ""
            bin_path = _bin_png_path(bin_dir, file_name, channel) if ri >= 0 else ""

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
                "binPath": bin_path,
                "channel": channel,
                "regionIndex": ri,
                "area": _to_float(rec.get("area", "")),
                "circularity": _to_float(rec.get("circularity", "")),
                "roundness": _to_float(rec.get("roundness", "")),
                "anisometry": _to_float(rec.get("anisometry", "")),
                "convexity": _to_float(rec.get("convexity", "")),
                "contlength": _to_float(rec.get("contlength", "")),
                "diameter": _to_float(rec.get("diameter", "")),
                # mm 파생 컬럼(--scale-x/--scale-y 제공 시에만 CSV 에 존재)
                "areaMm2": _to_float(rec.get("area_mm2", "")),
                "widthMm": _to_float(rec.get("width_mm", "")),
                "heightMm": _to_float(rec.get("height_mm", "")),
                "diameterMm": _to_float(rec.get("diameter_mm", "")),
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

    # 이진화 플래그 + 캐시 라벨(granular 우선, mode 비면 legacy binarize 폴백)
    bin_flags, bin_label = build_binarize(
        req.polarity, req.mode, req.thresh, req.offset, legacy_key=req.binarize,
        kernel=req.kernel, response=req.response, blur=req.blur,
        black_th=req.blackTh, white_th=req.whiteTh, proj_kernel=req.projKernel)

    is_proj = "--proj" in bin_flags

    # 픽셀→mm 스케일(공통). 1.0 이 아니고 exe 가 지원할 때만 전달(라벨에도 반영해 캐시 분리).
    scale_flags = []
    scale_label = ""
    try:
        sx = float(req.scaleX)
        sy = float(req.scaleY)
    except (TypeError, ValueError):
        sx, sy = 1.0, 1.0
    scale_on = (abs(sx - 1.0) > 1e-9 or abs(sy - 1.0) > 1e-9)
    if scale_on and exe_supports(exe, "--scale-x"):
        scale_flags = ["--scale-x", ("%g" % sx), "--scale-y", ("%g" % sy)]
        scale_label = "-sx%g-sy%g" % (sx, sy)

    # 실행 캐시 폴더 = 해시(폴더+프로파일+이진화+스케일)
    key = "%s|%s|%s%s" % (_norm(folder), profile_key, bin_label, scale_label)
    h = hashlib.md5(key.encode("utf-8")).hexdigest()[:16]
    out_dir = os.path.join(config.CACHE_DIR, h)
    overlay_dir = os.path.join(out_dir, "overlay")
    bin_dir = os.path.join(out_dir, "bin")
    os.makedirs(overlay_dir, exist_ok=True)
    csv_path = os.path.join(out_dir, "result.csv")

    # exe 인자 구성
    args = [exe, folder, csv_path]
    profile_ini = config.PROFILE_MAP.get(profile_key)
    if profile_ini:
        args.append(os.path.join(config.PROFILES_DIR, profile_ini))
    args += bin_flags
    args += scale_flags
    if req.threads and req.threads > 0:
        args += ["--threads", str(int(req.threads))]
    args += ["--overlay", overlay_dir]
    # 이진화 PNG 덤프(카드에 원본|이진화|오버레이 표시). exe 미지원 시 전달 생략.
    dump_on = exe_supports(exe, "--dumpbin")
    if dump_on:
        os.makedirs(bin_dir, exist_ok=True)
        args += ["--dumpbin", bin_dir]

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

    rows, columns, score_features = _parse_csv(
        csv_path, overlay_dir, bin_dir=(bin_dir if dump_on else None))
    agg = _summarize(rows, score_features)

    # 이미지 서빙 허용 폴더 등록
    ALLOWED_DIRS.add(_norm(folder))

    has_mm = any(c in columns for c in ("area_mm2", "width_mm", "height_mm", "diameter_mm"))
    has_channel = ("Channel" in columns)

    # 결과 자동 저장(DB 설정 있을 때만; 실패/비활성 시 조용히 건너뜀 — 분석 자체는 영향 없음)
    run_id = None
    try:
        total_regions = sum(1 for r in rows if int(r.get("regionIndex", -1)) >= 0)
        total_files = len({r.get("fileName") for r in rows if r.get("fileName")})
        meta = {
            "folder": folder,
            "profile": profile_key,
            "binarize": bin_label,
            "totalFiles": total_files,
            "totalRegions": total_regions,
            "params": {
                "polarity": req.polarity, "mode": req.mode,
                "thresh": req.thresh, "offset": req.offset,
                "scaleX": sx, "scaleY": sy, "cacheHash": h,
            },
        }
        run_id = db.insert_run(meta, rows)
    except Exception:
        run_id = None

    return {
        "ok": True,
        "hash": h,
        "profile": profile_key,
        "binarize": bin_label,
        "isProjection": is_proj,
        "hasChannel": has_channel,
        "hasMm": has_mm,
        "scaleX": sx,
        "scaleY": sy,
        "columns": columns,
        "scoreFeatures": score_features,
        "summary": dict(agg["summary"], elapsedMs=exe_elapsed if exe_elapsed is not None else elapsed_ms),
        "defectDistribution": agg["defectDistribution"],
        "scoreByCode": agg["scoreByCode"],
        "rows": rows,
        "runId": run_id,
        "dbSaved": run_id is not None,
    }


# ===================== 결과 이력(History) API =====================
@app.get("/api/runs")
def api_runs(limit: int = Query(200)):
    """저장된 실행 이력(최근순). DB 비활성 시 enabled=false + 빈 목록."""
    if not db.is_enabled():
        return {"ok": True, "enabled": False, "runs": []}
    return {"ok": True, "enabled": True, "runs": db.list_runs(limit=max(1, min(1000, limit)))}


@app.get("/api/runs/{run_id}")
def api_run_detail(run_id: int):
    """실행 1건의 메타 + Region별 feature 값."""
    if not db.is_enabled():
        return JSONResponse({"ok": False, "error": "DB가 비활성 상태입니다."}, status_code=400)
    data = db.get_run(run_id)
    if data is None:
        return JSONResponse({"ok": False, "error": f"run {run_id} 없음"}, status_code=404)
    return {"ok": True, "run": data["run"], "regions": data["regions"]}


# ===================== 군집(Cluster) API =====================
# 군집 계산은 C++ Batch(cv::kmeans)가 유일 소스. 웹은 라벨 수신 + raw 2축 산점도만.
#  프리셋 "shape8" = 뷰어와 동일한 형상 8종. "all" = 형상 8종 + 크기 파생.
SHAPE8_FEATURES = ["circularity", "compactness", "convexity", "rectangularity",
                   "roundness", "anisometry", "bulkiness", "structure_factor"]
ALL_CLUSTER_FEATURES = (["area", "contlength", "diameter"] + SHAPE8_FEATURES +
                        ["ra", "rb", "roundness_distance", "roundness_sigma"])


def _resolve_features(spec):
    """features 스펙(쉼표 문자열 또는 프리셋) → feature 이름 리스트."""
    s = (spec or "").strip().lower()
    if s in ("", "shape8"):
        return list(SHAPE8_FEATURES)
    if s == "all":
        return list(ALL_CLUSTER_FEATURES)
    feats = [x.strip() for x in str(spec).split(",") if x.strip()]
    return feats or list(SHAPE8_FEATURES)


def _read_cluster_feature_rows(csv_path, feature_cols):
    """CSV 의 Region 행만(순서 보존) 읽어 선택 feature 값 배열을 뽑는다.
    엔진(ClusterCsvIO)이 RegionIndex<0(NO_REGION) 행을 제외하므로 여기서도 동일하게 제외해야
    assignments 와 행 순서가 1:1 로 맞는다(fileName 매칭 아님)."""
    rows = []
    with open(csv_path, "r", newline="", encoding="utf-8", errors="replace") as f:
        reader = csv.DictReader(f)
        for rec in reader:
            try:
                ri = int(float(rec.get("RegionIndex", "")))
            except (TypeError, ValueError):
                ri = -1
            if ri < 0:
                continue  # NO_REGION 행 제외(엔진과 동일)
            vals = [_to_float(rec.get(c, "")) for c in feature_cols]
            rows.append({
                "file": rec.get("FileName", ""),
                "path": (rec.get("FilePath", "") or "").strip(),  # hover 이미지 툴팁용 원본 경로
                "regionIndex": ri,
                "channel": (rec.get("Channel", "") or "").strip(),
                "values": vals,
            })
    return rows


@app.post("/api/cluster")
def api_cluster(req: ClusterRequest):
    folder = req.folderPath.strip().strip('"')
    if not folder or not os.path.isdir(folder):
        return JSONResponse({"ok": False, "error": f"폴더를 찾을 수 없습니다: {folder}"}, status_code=400)
    ALLOWED_DIRS.add(_norm(folder))  # hover 이미지 툴팁(/api/image) 서빙 허용

    exe = config.find_batch_exe()
    if not exe:
        return JSONResponse({"ok": False, "error": "GlimRegionBatch.exe 를 찾을 수 없습니다. 먼저 Release 빌드가 필요합니다."}, status_code=500)

    # 이진화 플래그 + 캐시 라벨(설정 탭 재사용, /api/run 과 동일 규칙)
    bin_flags, bin_label = build_binarize(
        req.polarity, req.mode, req.thresh, req.offset, legacy_key=req.binarize,
        kernel=req.kernel, response=req.response, blur=req.blur,
        black_th=req.blackTh, white_th=req.whiteTh, proj_kernel=req.projKernel)

    features = _resolve_features(req.features)
    scale_mode = (req.scaleMode or "zscore").lower()
    if scale_mode not in ("zscore", "minmax", "robust"):
        scale_mode = "zscore"
    try:
        k = int(req.k)
    except (TypeError, ValueError):
        k = 0
    if k < 0:
        k = 0
    try:
        k_max = int(req.kMax)
    except (TypeError, ValueError):
        k_max = 8
    k_max = max(2, min(12, k_max))

    # 특징 CSV 캐시(폴더+이진화). 군집 결과는 그 안에 군집 파라미터별 파일로 분리.
    csv_key = "cluster|%s|%s" % (_norm(folder), bin_label)
    h_csv = hashlib.md5(csv_key.encode("utf-8")).hexdigest()[:16]
    out_dir = os.path.join(config.CACHE_DIR, "cluster", h_csv)
    os.makedirs(out_dir, exist_ok=True)
    csv_path = os.path.join(out_dir, "features.csv")

    env = dict(os.environ)
    env["PATH"] = config.OPENCV_BIN + os.pathsep + env.get("PATH", "")

    # 1) 특징 CSV 생성(프로파일 없음 — 군집엔 raw 특징값만 필요). 캐시 있으면 재사용.
    if not os.path.isfile(csv_path):
        args = [exe, folder, csv_path]
        args += bin_flags
        if req.threads and req.threads > 0:
            args += ["--threads", str(int(req.threads))]
        try:
            proc = subprocess.run(args, env=env, capture_output=True, text=True, timeout=600)
        except Exception as e:
            return JSONResponse({"ok": False, "error": f"특징 추출 exe 실행 실패: {e}"}, status_code=500)
        if not os.path.isfile(csv_path):
            return JSONResponse({
                "ok": False, "error": "특징 CSV 가 생성되지 않았습니다.",
                "stdout": proc.stdout, "stderr": proc.stderr,
            }, status_code=500)

    # 2) --cluster 로 clusters.json 생성(군집 계산 단일 소스 = C++)
    clu_key = "%s|%s|k%d|kmax%d" % (",".join(features), scale_mode, k, k_max)
    h_clu = hashlib.md5(clu_key.encode("utf-8")).hexdigest()[:16]
    clusters_json = os.path.join(out_dir, "clusters_%s.json" % h_clu)
    cargs = [exe, "--cluster", csv_path, clusters_json,
             "--features", ",".join(features),
             "--k", (str(k) if k > 0 else "auto"),
             "--kmax", str(k_max),
             "--scale", scale_mode]
    t0 = time.time()
    try:
        cproc = subprocess.run(cargs, env=env, capture_output=True, text=True, timeout=600)
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"군집화 exe 실행 실패: {e}"}, status_code=500)
    elapsed_ms = int((time.time() - t0) * 1000)

    if not os.path.isfile(clusters_json):
        return JSONResponse({
            "ok": False, "error": "clusters.json 이 생성되지 않았습니다.",
            "stdout": cproc.stdout, "stderr": cproc.stderr,
        }, status_code=500)

    try:
        with open(clusters_json, "r", encoding="utf-8", errors="replace") as f:
            cj = json.load(f)
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"clusters.json 파싱 실패: {e}"}, status_code=500)

    assignments = cj.get("assignments") or []
    labels = []
    for a in assignments:
        try:
            labels.append(int(a.get("cluster", -1)))
        except (TypeError, ValueError):
            labels.append(-1)

    # 실제 사용된 feature = scaler.features(CSV 헤더에 존재하는 것만). 산점도 축 후보.
    scaler = cj.get("scaler") or {}
    used_features = scaler.get("features") or features
    if not isinstance(used_features, list) or not used_features:
        used_features = features

    # CSV Region 행(순서 보존) + assignments 를 행 인덱스로 1:1 병합
    try:
        feat_rows = _read_cluster_feature_rows(csv_path, used_features)
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"특징 CSV 병합 실패: {e}"}, status_code=500)

    n = min(len(feat_rows), len(labels))
    points = []
    for i in range(n):
        r = feat_rows[i]
        points.append({
            "file": r["file"],
            "path": r["path"],
            "regionIndex": r["regionIndex"],
            "channel": r["channel"],
            "cluster": labels[i],
            "values": r["values"],
        })

    cluster_sizes = cj.get("clusterSizes") or []
    result_k = cj.get("k")
    if result_k is None:
        result_k = len(cluster_sizes)
    silhouette = cj.get("silhouette")

    return {
        "ok": True,
        "columns": used_features,
        "points": points,
        "k": result_k,
        "silhouette": silhouette,
        "clusterSizes": cluster_sizes,
        "features": used_features,
        "scaleMode": scale_mode,
        "elapsedMs": elapsed_ms,
        "rowMismatch": (len(feat_rows) != len(labels)),
    }


# ===================== 프리셋 API =====================
@app.get("/api/presets")
def api_presets_list():
    """저장된 프리셋 목록. (없으면 기본 2종 생성 후 반환)"""
    try:
        return {"ok": True, "presets": _load_presets()}
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"프리셋 목록을 불러오지 못했습니다: {e}"}, status_code=500)


@app.post("/api/presets")
def api_presets_save(req: PresetSaveRequest):
    """프리셋 저장(동일 이름이면 덮어쓰기). builtin 프리셋 이름은 덮어쓸 수 없다."""
    name = (req.name or "").strip()
    if not name:
        return JSONResponse({"ok": False, "error": "프리셋 이름을 입력하세요."}, status_code=400)
    if len(name) > 60:
        return JSONResponse({"ok": False, "error": "프리셋 이름이 너무 깁니다(최대 60자)."}, status_code=400)
    settings = _sanitize_preset_settings(req.settings)
    if not settings:
        return JSONResponse({"ok": False, "error": "저장할 설정이 비어 있습니다."}, status_code=400)
    try:
        with _PRESETS_LOCK:
            presets = _load_presets_unlocked()
            for p in presets:
                if p.get("name") == name and p.get("builtin"):
                    return JSONResponse(
                        {"ok": False, "error": "기본 제공 프리셋 이름은 덮어쓸 수 없습니다. 다른 이름을 사용하세요."},
                        status_code=400)
            presets = [p for p in presets if p.get("name") != name]
            presets.append({"name": name, "settings": settings})
            _write_presets_unlocked(presets)
        return {"ok": True, "name": name, "presets": presets}
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"프리셋 저장 실패: {e}"}, status_code=500)


@app.delete("/api/presets")
def api_presets_delete(name: str = Query(...)):
    """프리셋 삭제. builtin 프리셋은 삭제 불가."""
    name = (name or "").strip()
    if not name:
        return JSONResponse({"ok": False, "error": "삭제할 프리셋 이름이 없습니다."}, status_code=400)
    try:
        with _PRESETS_LOCK:
            presets = _load_presets_unlocked()
            target = next((p for p in presets if p.get("name") == name), None)
            if target is None:
                return JSONResponse({"ok": False, "error": f"프리셋을 찾을 수 없습니다: {name}"}, status_code=404)
            if target.get("builtin"):
                return JSONResponse({"ok": False, "error": "기본 제공 프리셋은 삭제할 수 없습니다."}, status_code=400)
            presets = [p for p in presets if p.get("name") != name]
            _write_presets_unlocked(presets)
        return {"ok": True, "name": name, "presets": presets}
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"프리셋 삭제 실패: {e}"}, status_code=500)


@app.get("/api/download")
def api_download(hash: str = Query(...)):
    """방금 실행 결과 CSV 를 그대로 다운로드. hash 는 /api/run 응답의 캐시 해시."""
    h = (hash or "").strip().lower()
    # 해시는 md5[:16] hex → 16자리 hex 만 허용(path traversal 방어)
    if not re.fullmatch(r"[0-9a-f]{16}", h):
        return JSONResponse({"ok": False, "error": "잘못된 결과 식별자입니다."}, status_code=400)
    csv_path = os.path.join(config.CACHE_DIR, h, "result.csv")
    if not os.path.isfile(csv_path):
        return JSONResponse({"ok": False, "error": "다운로드할 결과 CSV 가 없습니다. 먼저 분석을 실행하세요."}, status_code=404)
    return FileResponse(csv_path, media_type="text/csv",
                        filename="GlimRegion_result_%s.csv" % h,
                        headers={"Cache-Control": "no-store"})


@app.get("/api/image")
def api_image(path: str = Query(...), overlay: int = 0):
    if not _is_allowed_image_path(path):
        return JSONResponse({"error": "허용되지 않은 경로"}, status_code=403)
    if not os.path.isfile(path):
        return JSONResponse({"error": "파일 없음"}, status_code=404)
    return FileResponse(path)


@app.get("/api/files")
def api_files(folder: str = Query(...), n: int = 5):
    """폴더의 앞쪽 이미지 파일명 N개(미리보기 대상 선택용). 폴더도 이미지 서빙 허용에 등록."""
    folder = (folder or "").strip().strip('"')
    if not folder or not os.path.isdir(folder):
        return JSONResponse({"ok": False, "error": f"폴더를 찾을 수 없습니다: {folder}"}, status_code=400)
    names = _list_images(folder, n if n and n > 0 else None)
    ALLOWED_DIRS.add(_norm(folder))  # 원본 미리보기(/api/image) 서빙 허용
    return {"ok": True, "folder": folder, "files": names}


@app.get("/api/preview")
def api_preview(
    folder: str = Query(...),
    file: str = Query(...),
    polarity: str = "bright",
    mode: str = "fixed",
    thresh: int = 127,
    offset: int = 20,
    kernel: int = 15,
    blur: int = 31,
    response: int = 4,
    blackTh: int = 20,
    whiteTh: int = 235,
    projKernel: int = 3,
    binarize: str = "",
):
    """선택 1장의 이진화 결과 PNG 를 반환. 실제 배치와 동일하도록 exe --preview 재사용.
    projection 이면 exe 가 흑|백 2채널을 가로로 이어붙인 1장을 반환(프론트에서 좌/우 분할)."""
    folder = (folder or "").strip().strip('"')
    if not folder or not os.path.isdir(folder):
        return JSONResponse({"error": f"폴더를 찾을 수 없습니다: {folder}"}, status_code=400)

    # file 은 폴더 내 단일 파일명만 허용(path traversal 방어)
    fname = os.path.basename((file or "").strip().strip('"'))
    src_path = os.path.join(folder, fname)
    if not fname or not os.path.isfile(src_path):
        return JSONResponse({"error": f"파일을 찾을 수 없습니다: {fname}"}, status_code=404)

    exe = config.find_batch_exe()
    if not exe:
        return JSONResponse({"error": "GlimRegionBatch.exe 를 찾을 수 없습니다."}, status_code=500)

    bin_flags, bin_label = build_binarize(polarity, mode, thresh, offset, legacy_key=binarize,
        kernel=kernel, response=response, blur=blur,
        black_th=blackTh, white_th=whiteTh, proj_kernel=projKernel)

    # 미리보기 출력 캐시 파일(파라미터+파일 해시). 매번 덮어써도 무방하나 해시로 경합 회피.
    prev_dir = os.path.join(config.CACHE_DIR, "preview")
    os.makedirs(prev_dir, exist_ok=True)
    pkey = "%s|%s|%s" % (_norm(src_path), bin_label, fname)
    ph = hashlib.md5(pkey.encode("utf-8")).hexdigest()[:16]
    out_png = os.path.join(prev_dir, ph + ".png")

    # exe --preview <입력파일> <출력png> [이진화 플래그]
    #  위치인자(입력폴더/출력csv)는 미리보기 모드에서 사용되지 않으나 형식상 채움.
    args = [exe, folder, os.path.join(prev_dir, "_dummy.csv"),
            "--preview", src_path, out_png] + bin_flags

    env = dict(os.environ)
    env["PATH"] = config.OPENCV_BIN + os.pathsep + env.get("PATH", "")
    try:
        proc = subprocess.run(args, env=env, capture_output=True, text=True, timeout=60)
    except Exception as e:
        return JSONResponse({"error": f"exe 실행 실패: {e}"}, status_code=500)

    if not os.path.isfile(out_png):
        return JSONResponse({
            "error": "미리보기 생성 실패",
            "stdout": proc.stdout, "stderr": proc.stderr,
        }, status_code=500)

    # TH/offset 변경 시 새로고침되도록 캐시 방지
    return FileResponse(out_png, media_type="image/png", headers={
        "Cache-Control": "no-store, must-revalidate",
        "X-Binarize-Label": bin_label,
        "X-Projection": "1" if ("--proj" in bin_flags) else "0",
    })


if __name__ == "__main__":
    import uvicorn
    os.makedirs(config.CACHE_DIR, exist_ok=True)
    uvicorn.run(app, host=config.HOST, port=config.PORT)
