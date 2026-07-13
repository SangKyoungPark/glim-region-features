@echo off
REM run.bat - GlimRegion 웹 대시보드 실행 (ASCII only to be console-safe)
setlocal
cd /d "%~dp0"

set "VENV=.venv"

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
echo  GlimRegion Dashboard  ->  http://localhost:8080
echo  (Ctrl+C to stop)
echo ================================================================
echo.

start "" http://localhost:8080
"%VENV%\Scripts\python.exe" -m uvicorn main:app --host 127.0.0.1 --port 8080

endlocal
