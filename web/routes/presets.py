# routes/presets.py - 설정 프리셋 API (web/presets.json 영속화)
#
# 프리셋 = {name, settings{...}, builtin?}. settings 는 프론트 readSettings() 형태 그대로.
# 파일 동시 접근 방어용 락(단일 프로세스 다중 요청 대비).
import os
import json
import threading

from fastapi import APIRouter, Query
from fastapi.responses import JSONResponse
from pydantic import BaseModel

import config

router = APIRouter()

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


@router.get("/api/presets")
def api_presets_list():
    """저장된 프리셋 목록. (없으면 기본 2종 생성 후 반환)"""
    try:
        return {"ok": True, "presets": _load_presets()}
    except Exception as e:
        return JSONResponse({"ok": False, "error": f"프리셋 목록을 불러오지 못했습니다: {e}"}, status_code=500)


@router.post("/api/presets")
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


@router.delete("/api/presets")
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
