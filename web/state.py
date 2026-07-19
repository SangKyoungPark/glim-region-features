# state.py - 라우터 간 공유 상태(이미지 서빙 허용 폴더) + 경로 정규화 유틸
#
# /api/run, /api/cluster, /api/files 가 실행/조회한 입력 폴더를 여기 등록하면
# /api/image 가 그 폴더(및 캐시 폴더) 내부 파일만 서빙하도록 허용한다(path traversal 방어).
# 단일 프로세스 내 여러 라우터가 같은 집합을 공유해야 하므로 모듈 전역으로 둔다.
import os

import config

# 실행/조회했던 입력 폴더(정규화 절대경로) 집합. /api/image 경로 방어용.
ALLOWED_DIRS = set()


def norm(p):
    return os.path.normcase(os.path.normpath(os.path.abspath(p)))


def allow_dir(p):
    """입력 폴더를 이미지 서빙 허용 목록에 등록."""
    ALLOWED_DIRS.add(norm(p))


def is_allowed_image_path(path):
    """캐시 폴더 내부 또는 실행했던 입력 폴더 내부만 허용(path traversal 방어)."""
    np = norm(path)
    cache = norm(config.CACHE_DIR)
    if np.startswith(cache + os.sep) or np == cache:
        return True
    for d in ALLOWED_DIRS:
        if np.startswith(d + os.sep) or np == d:
            return True
    return False
