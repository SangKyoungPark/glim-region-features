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

# OpenCV DLL 경로 (exe 실행 시 PATH 앞에 추가)
OPENCV_BIN = r"C:\Lib\opencv\build\x64\vc16\bin"

# 서버 포트
HOST = "127.0.0.1"
PORT = 8080


def find_batch_exe():
    for p in BATCH_EXE_CANDIDATES:
        if os.path.isfile(p):
            return p
    return None
