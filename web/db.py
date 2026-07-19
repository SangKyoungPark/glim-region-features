# db.py - 검사 결과 PostgreSQL 영속화 (psycopg3)
#
# - 설정(db_config.json 또는 환경변수 GRF_PG_DSN)이 없거나 접속 불가면 모든 함수가
#   안전하게 no-op(None/[]) 를 반환한다 → 웹은 DB 없이도 그대로 동작(우아한 저하).
# - 스키마: runs(실행 메타) 1 : N regions(Region별 feature). 전체 원시 feature 는 JSONB.
# - 비밀번호 등 접속 정보는 db_config.json(gitignore) 또는 환경변수로만 관리(코드/커밋 금지).
import os
import json
import datetime

import config

try:
    import psycopg
    from psycopg.types.json import Jsonb
except Exception:  # 드라이버 미설치 시에도 웹은 동작
    psycopg = None
    Jsonb = None

_CFG_PATH = os.path.join(config.BASE_DIR, "db_config.json")

RUNS_DDL = """
CREATE TABLE IF NOT EXISTS runs (
    id             BIGSERIAL PRIMARY KEY,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
    folder         TEXT NOT NULL,
    profile        TEXT,
    binarize_label TEXT,
    total_files    INTEGER,
    total_regions  INTEGER,
    params         JSONB
)
"""

REGIONS_DDL = """
CREATE TABLE IF NOT EXISTS regions (
    id              BIGSERIAL PRIMARY KEY,
    run_id          BIGINT NOT NULL REFERENCES runs(id) ON DELETE CASCADE,
    file_name       TEXT,
    file_path       TEXT,
    region_index    INTEGER,
    channel         TEXT,
    area            DOUBLE PRECISION,
    circularity     DOUBLE PRECISION,
    convexity       DOUBLE PRECISION,
    roundness       DOUBLE PRECISION,
    anisometry      DOUBLE PRECISION,
    classified_code TEXT,
    features        JSONB,
    scores          JSONB
)
"""

INDEX_DDL = [
    "CREATE INDEX IF NOT EXISTS idx_regions_run ON regions(run_id)",
    "CREATE INDEX IF NOT EXISTS idx_regions_code ON regions(classified_code)",
    "CREATE INDEX IF NOT EXISTS idx_runs_created ON runs(created_at DESC)",
]


def load_config():
    """접속 설정 반환. 우선순위: 환경변수 GRF_PG_DSN > db_config.json. 없으면 None(=DB 비활성)."""
    dsn = os.environ.get("GRF_PG_DSN")
    if dsn:
        return {"conninfo": dsn}
    if os.path.isfile(_CFG_PATH):
        try:
            with open(_CFG_PATH, "r", encoding="utf-8") as f:
                c = json.load(f)
            keys = ("host", "port", "user", "password", "dbname")
            return {"kwargs": {k: c[k] for k in keys if k in c}}
        except Exception:
            return None
    return None


def is_enabled():
    return psycopg is not None and load_config() is not None


def get_conn():
    if psycopg is None:
        return None
    cfg = load_config()
    if not cfg:
        return None
    try:
        if "conninfo" in cfg:
            return psycopg.connect(cfg["conninfo"], connect_timeout=5)
        return psycopg.connect(connect_timeout=5, **cfg["kwargs"])
    except Exception:
        return None


def ensure_schema():
    """테이블/인덱스 생성(멱등). 성공 시 True."""
    conn = get_conn()
    if conn is None:
        return False
    try:
        with conn:
            with conn.cursor() as cur:
                cur.execute(RUNS_DDL)
                cur.execute(REGIONS_DDL)
                for ddl in INDEX_DDL:
                    cur.execute(ddl)
        return True
    except Exception:
        return False
    finally:
        conn.close()


def insert_run(meta, rows):
    """실행 1건 + Region들을 저장하고 run_id 를 반환. 실패/비활성 시 None."""
    conn = get_conn()
    if conn is None:
        return None
    try:
        with conn:
            with conn.cursor() as cur:
                cur.execute(
                    "INSERT INTO runs(folder, profile, binarize_label, total_files, total_regions, params) "
                    "VALUES (%s, %s, %s, %s, %s, %s) RETURNING id",
                    (meta.get("folder"), meta.get("profile"), meta.get("binarize"),
                     meta.get("totalFiles"), meta.get("totalRegions"),
                     Jsonb(meta.get("params") or {})),
                )
                run_id = cur.fetchone()[0]

                data = []
                for r in rows:
                    if int(r.get("regionIndex", -1)) < 0:
                        continue  # NO_REGION 행 제외
                    data.append((
                        run_id, r.get("fileName"), r.get("filePath"),
                        r.get("regionIndex"), r.get("channel"),
                        r.get("area"), r.get("circularity"), r.get("convexity"),
                        r.get("roundness"), r.get("anisometry"),
                        r.get("classifiedCode"),
                        Jsonb(r.get("raw") or {}), Jsonb(r.get("scores") or {}),
                    ))
                if data:
                    cur.executemany(
                        "INSERT INTO regions(run_id, file_name, file_path, region_index, channel, "
                        "area, circularity, convexity, roundness, anisometry, classified_code, features, scores) "
                        "VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)",
                        data,
                    )
        return run_id
    except Exception:
        return None
    finally:
        conn.close()


def _iso(v):
    return v.isoformat() if isinstance(v, (datetime.datetime, datetime.date)) else v


def list_runs(limit=200):
    """최근 실행 목록(내림차순). 비활성 시 []."""
    conn = get_conn()
    if conn is None:
        return []
    try:
        with conn.cursor() as cur:
            cur.execute(
                "SELECT id, created_at, folder, profile, binarize_label, total_files, total_regions "
                "FROM runs ORDER BY id DESC LIMIT %s", (limit,))
            cols = [d.name for d in cur.description]
            out = []
            for row in cur.fetchall():
                d = dict(zip(cols, row))
                d["created_at"] = _iso(d.get("created_at"))
                out.append(d)
            return out
    except Exception:
        return []
    finally:
        conn.close()


def get_run(run_id, limit=10000):
    """실행 1건의 메타 + Region 목록. 없으면 None."""
    conn = get_conn()
    if conn is None:
        return None
    try:
        with conn.cursor() as cur:
            cur.execute(
                "SELECT id, created_at, folder, profile, binarize_label, total_files, total_regions "
                "FROM runs WHERE id=%s", (run_id,))
            r = cur.fetchone()
            if not r:
                return None
            rcols = [d.name for d in cur.description]
            run = dict(zip(rcols, r))
            run["created_at"] = _iso(run.get("created_at"))

            cur.execute(
                "SELECT file_name, region_index, channel, area, circularity, convexity, "
                "roundness, anisometry, classified_code, features, scores "
                "FROM regions WHERE run_id=%s ORDER BY id LIMIT %s", (run_id, limit))
            gcols = [d.name for d in cur.description]
            regions = [dict(zip(gcols, row)) for row in cur.fetchall()]
            return {"run": run, "regions": regions}
    except Exception:
        return None
    finally:
        conn.close()
