@echo off
REM run.bat - GlimRegion 웹 대시보드 실행 (ASCII only to be console-safe)
REM   usage: run.bat [PORT]
REM     PORT 인자를 주면 그 포트로, 없으면 환경변수 GRF_WEB_PORT, 그것도 없으면 8080.
setlocal
cd /d "%~dp0"

set "VENV=.venv"

REM ---- 포트 결정: 인자 > 기존 GRF_WEB_PORT > 8080 ----
set "PORT=%~1"
if "%PORT%"=="" set "PORT=%GRF_WEB_PORT%"
if "%PORT%"=="" set "PORT=8080"
set "GRF_WEB_PORT=%PORT%"

if not exist "%VENV%\Scripts\python.exe" (
  echo [GlimRegion] Creating virtual environment ...
  python -m venv "%VENV%"
  if errorlevel 1 (
    echo [GlimRegion] ERROR: failed to create venv. Is Python 3 on PATH?
    pause
    exit /b 1
  )
)

echo [GlimRegion] Installing requirements ...
"%VENV%\Scripts\python.exe" -m pip install --quiet --disable-pip-version-check -r requirements.txt
if errorlevel 1 (
  echo [GlimRegion] ERROR: pip install failed.
  pause
  exit /b 1
)

echo.
echo ================================================================
echo  GlimRegion Dashboard  -^>  http://localhost:%PORT%
echo  (Ctrl+C to stop)
echo ================================================================
echo.

start "" http://localhost:%PORT%
"%VENV%\Scripts\python.exe" -m uvicorn main:app --host 127.0.0.1 --port %PORT%

endlocal
