# main.py - GlimRegion 로컬 분석 대시보드 (FastAPI) 진입점
#
# 브라우저 → 이 서버 → GlimRegionBatch.exe 실행 → CSV+오버레이 파싱 → JSON.
# C++ 계산 엔진 재사용, 웹은 표시만. (같은 PC 전제, 폴더는 서버 로컬 경로 텍스트 입력)
#
# 얇은 진입점: app 생성 + startup(DB 스키마) + StaticFiles 마운트 + 라우터 등록.
# 실제 엔드포인트는 web/routes/ 패키지, 공용 로직은 engine/state/binarize/csv_parse 모듈.
#
# 실행: web/run.bat  또는  python -m uvicorn main:app --host 127.0.0.1 --port 8080
import os

from fastapi import FastAPI
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles

import config
import db  # 검사 결과 PostgreSQL 저장(설정 없으면 no-op)
from routes import run as run_routes
from routes import cluster as cluster_routes
from routes import history as history_routes
from routes import presets as presets_routes

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

# 기능별 라우터 등록
app.include_router(run_routes.router)
app.include_router(cluster_routes.router)
app.include_router(history_routes.router)
app.include_router(presets_routes.router)


@app.get("/", response_class=HTMLResponse)
def index():
    idx = os.path.join(STATIC_DIR, "index.html")
    if not os.path.isfile(idx):
        return HTMLResponse("<h1>GlimRegion Dashboard</h1><p>static/index.html 준비 중</p>")
    with open(idx, "r", encoding="utf-8") as f:
        return HTMLResponse(f.read())


if __name__ == "__main__":
    import uvicorn
    os.makedirs(config.CACHE_DIR, exist_ok=True)
    uvicorn.run(app, host=config.HOST, port=config.PORT)
