# engine.py - GlimRegionBatch.exe 실행 보일러플레이트 통합
#
# exe 탐색 + OpenCV DLL PATH 주입 + subprocess.run 을 한 곳으로 모은다.
# 라우터들은 run_batch()/exe_supports()/find_exe() 만 사용하고,
# 환경변수 조립·서브프로세스 호출 세부는 이 모듈이 책임진다.
import os
import subprocess

import config


def find_exe():
    """GlimRegionBatch.exe 경로 반환(없으면 None). config.find_batch_exe 위임."""
    return config.find_batch_exe()


def _build_env():
    """OpenCV DLL 폴더를 PATH 앞에 추가한 환경 dict 반환(exe 실행 공통)."""
    env = dict(os.environ)
    env["PATH"] = config.OPENCV_BIN + os.pathsep + env.get("PATH", "")
    return env


def run_batch(args, timeout=600):
    """exe 를 인자 args(= [exe, ...])로 실행하고 CompletedProcess 반환.
    OpenCV PATH 주입 + capture_output + text 를 공통 처리한다.
    subprocess 예외(타임아웃 포함)는 그대로 전파 → 호출부가 상황별 메시지로 처리."""
    env = _build_env()
    return subprocess.run(args, env=env, capture_output=True, text=True, timeout=timeout)


# exe usage 문자열 캐시 → 신규 플래그(--dumpbin/--scale-x 등) 지원 여부 판별.
# 엔진 병행 작업이 아직 미반영이면 해당 플래그는 전달하지 않고 무해하게 폴백한다.
_EXE_CAPS = {}


def exe_supports(exe, flag):
    """exe 인자 없이 실행 → usage 출력에 flag 문자열이 있으면 지원으로 간주(결과 캐시)."""
    caps = _EXE_CAPS.get(exe)
    if caps is None:
        caps = ""
        try:
            p = run_batch([exe], timeout=15)
            caps = (p.stdout or "") + (p.stderr or "")
        except Exception:
            caps = ""
        _EXE_CAPS[exe] = caps
    return flag in caps
