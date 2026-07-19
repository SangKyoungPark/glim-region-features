# routes/run.py - 분석 실행/미리보기/파일목록/이미지/다운로드 API
#
# /api/run     : 폴더 배치 분석(exe) → CSV/오버레이 파싱 → JSON + DB 자동 저장
# /api/preview : 선택 1장 이진화 미리보기 PNG (exe --preview)
# /api/files   : 폴더 앞쪽 이미지 파일명 N개
# /api/image   : 캐시/입력 폴더 내부 이미지 서빙(path traversal 방어)
# /api/download: 방금 실행 결과 CSV 다운로드
import os
import re
import time
import hashlib

from fastapi import APIRouter, Query
from fastapi.responses import JSONResponse, FileResponse
from pydantic import BaseModel

import config
import db  # 검사 결과 PostgreSQL 저장(설정 없으면 no-op)
import engine
import state
from binarize import build_binarize
from csv_parse import _parse_csv, _summarize

router = APIRouter()


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


@router.post("/api/run")
def api_run(req: RunRequest):
    folder = req.folderPath.strip().strip('"')
    if not folder or not os.path.isdir(folder):
        return JSONResponse({"ok": False, "error": f"폴더를 찾을 수 없습니다: {folder}"}, status_code=400)

    exe = engine.find_exe()
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
    if scale_on and engine.exe_supports(exe, "--scale-x"):
        scale_flags = ["--scale-x", ("%g" % sx), "--scale-y", ("%g" % sy)]
        scale_label = "-sx%g-sy%g" % (sx, sy)

    # 실행 캐시 폴더 = 해시(폴더+프로파일+이진화+스케일)
    key = "%s|%s|%s%s" % (state.norm(folder), profile_key, bin_label, scale_label)
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
    dump_on = engine.exe_supports(exe, "--dumpbin")
    if dump_on:
        os.makedirs(bin_dir, exist_ok=True)
        args += ["--dumpbin", bin_dir]

    # OpenCV DLL 을 PATH 에 추가하고 실행
    t0 = time.time()
    try:
        proc = engine.run_batch(args, timeout=600)
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
    state.allow_dir(folder)

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


@router.get("/api/download")
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


@router.get("/api/image")
def api_image(path: str = Query(...), overlay: int = 0):
    if not state.is_allowed_image_path(path):
        return JSONResponse({"error": "허용되지 않은 경로"}, status_code=403)
    if not os.path.isfile(path):
        return JSONResponse({"error": "파일 없음"}, status_code=404)
    return FileResponse(path)


@router.get("/api/files")
def api_files(folder: str = Query(...), n: int = 5):
    """폴더의 앞쪽 이미지 파일명 N개(미리보기 대상 선택용). 폴더도 이미지 서빙 허용에 등록."""
    folder = (folder or "").strip().strip('"')
    if not folder or not os.path.isdir(folder):
        return JSONResponse({"ok": False, "error": f"폴더를 찾을 수 없습니다: {folder}"}, status_code=400)
    names = _list_images(folder, n if n and n > 0 else None)
    state.allow_dir(folder)  # 원본 미리보기(/api/image) 서빙 허용
    return {"ok": True, "folder": folder, "files": names}


@router.get("/api/preview")
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

    exe = engine.find_exe()
    if not exe:
        return JSONResponse({"error": "GlimRegionBatch.exe 를 찾을 수 없습니다."}, status_code=500)

    bin_flags, bin_label = build_binarize(polarity, mode, thresh, offset, legacy_key=binarize,
        kernel=kernel, response=response, blur=blur,
        black_th=blackTh, white_th=whiteTh, proj_kernel=projKernel)

    # 미리보기 출력 캐시 파일(파라미터+파일 해시). 매번 덮어써도 무방하나 해시로 경합 회피.
    prev_dir = os.path.join(config.CACHE_DIR, "preview")
    os.makedirs(prev_dir, exist_ok=True)
    pkey = "%s|%s|%s" % (state.norm(src_path), bin_label, fname)
    ph = hashlib.md5(pkey.encode("utf-8")).hexdigest()[:16]
    out_png = os.path.join(prev_dir, ph + ".png")

    # exe --preview <입력파일> <출력png> [이진화 플래그]
    #  위치인자(입력폴더/출력csv)는 미리보기 모드에서 사용되지 않으나 형식상 채움.
    args = [exe, folder, os.path.join(prev_dir, "_dummy.csv"),
            "--preview", src_path, out_png] + bin_flags

    try:
        proc = engine.run_batch(args, timeout=60)
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
