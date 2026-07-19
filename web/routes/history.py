# routes/history.py - 결과 이력(History) API (PostgreSQL)
#
# 저장된 실행 이력 목록/상세/코드 목록. DB 비활성 시 enabled=false + 빈 목록으로 우아하게 저하.
from fastapi import APIRouter, Query
from fastapi.responses import JSONResponse

import db

router = APIRouter()


@router.get("/api/runs")
def api_runs(limit: int = Query(200),
             date_from: str = Query(None), date_to: str = Query(None),
             folder: str = Query(None), code: str = Query(None)):
    """저장된 실행 이력(최근순). 필터: 기간(date_from~date_to), 폴더(부분일치), 코드.
    DB 비활성 시 enabled=false + 빈 목록."""
    if not db.is_enabled():
        return {"ok": True, "enabled": False, "runs": []}
    runs = db.list_runs(
        limit=max(1, min(1000, limit)),
        date_from=(date_from or None), date_to=(date_to or None),
        folder=(folder.strip() if folder else None),
        code=(code.strip() if code else None))
    return {"ok": True, "enabled": True, "runs": runs}


@router.get("/api/codes")
def api_codes():
    """저장된 분류 코드 목록(코드 필터 드롭다운용)."""
    if not db.is_enabled():
        return {"ok": True, "enabled": False, "codes": []}
    return {"ok": True, "enabled": True, "codes": db.distinct_codes()}


@router.get("/api/runs/{run_id}")
def api_run_detail(run_id: int):
    """실행 1건의 메타 + Region별 feature 값."""
    if not db.is_enabled():
        return JSONResponse({"ok": False, "error": "DB가 비활성 상태입니다."}, status_code=400)
    data = db.get_run(run_id)
    if data is None:
        return JSONResponse({"ok": False, "error": f"run {run_id} 없음"}, status_code=404)
    return {"ok": True, "run": data["run"], "regions": data["regions"]}
