# config.py - 로컬 웹 대시보드 설정
# 경로/상수는 이 파일에서 관리한다. (같은 PC 전제, 서버 로컬 경로 방식)
import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))       # web/
REPO_DIR = os.path.dirname(BASE_DIR)                         # 저장소 루트

# GlimRegionBatch.exe 후보 (Release 우선)
BATCH_EXE_CANDIDATES = [
    os.path.join(REPO_DIR, "bin", "x64", "Release", "GlimRegionBatch.exe"),
    os.path.join(REPO_DIR, "bin", "x64", "Debug", "GlimRegionBatch.exe"),
]

# 프로파일 INI 폴더
PROFILES_DIR = os.path.join(REPO_DIR, "profiles")

# 프로파일 이름 → INI 파일
PROFILE_MAP = {
    "coater": "Coater.ini",
    "rollpress": "RollPress.ini",
    "slitter": "Slitter.ini",
    "none": None,
}

# 이진화 모드 → GlimRegionBatch CLI 인자
BINARIZE_MAP = {
    "bright127": [],                          # 기본(고정 127, 밝은 쪽)
    "dark127":   ["--dark"],                  # 고정 127, 어두운 쪽
    "darkauto":  ["--dark", "--thresh", "auto"],
    "brightauto":["--bright", "--thresh", "auto"],
}

# 실행 결과 캐시 폴더 (CSV + 오버레이 PNG)
CACHE_DIR = os.path.join(BASE_DIR, "cache")

# 설정 프리셋 영속화 파일 (이름 붙인 설정 저장/불러오기)
PRESETS_FILE = os.path.join(BASE_DIR, "presets.json")

# OpenCV DLL 경로 (exe 실행 시 PATH 앞에 추가)
OPENCV_BIN = r"C:\Lib\opencv\build\x64\vc16\bin"

# 서버 포트/호스트
#  - 우선순위: 환경변수 GRF_WEB_PORT > 기본값 8080 (run.bat 인자로도 GRF_WEB_PORT 를 set 한다)
#  - 호스트는 같은 PC 전제이므로 루프백 고정.
HOST = "127.0.0.1"


def _read_port(default=8080):
    """환경변수 GRF_WEB_PORT 를 읽어 1~65535 범위 정수면 사용, 아니면 기본값."""
    raw = os.environ.get("GRF_WEB_PORT", "").strip()
    if not raw:
        return default
    try:
        p = int(raw)
    except (TypeError, ValueError):
        return default
    return p if 1 <= p <= 65535 else default


PORT = _read_port()


def find_batch_exe():
    # 1) 환경변수 GRF_BATCH_EXE 로 명시 지정한 경로가 있으면 우선(빌드 위치가 다른 경우 대응)
    override = os.environ.get("GRF_BATCH_EXE", "").strip().strip('"')
    if override and os.path.isfile(override):
        return override
    # 2) 저장소 표준 후보(Release → Debug)
    for p in BATCH_EXE_CANDIDATES:
        if os.path.isfile(p):
            return p
    return None
