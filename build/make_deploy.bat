@echo off
REM ============================================================
REM  make_deploy.bat - Collect GlimRegionViewer release into
REM                    deploy\GlimRegionViewer_vX.X.X\ and zip it.
REM  Does NOT build. Build Release (x64) first, then run this.
REM ============================================================
setlocal ENABLEEXTENSIONS

set "VERSION=1.0.0"
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "ROOT=%%~fI"

set "SRC=%ROOT%\bin\x64\Release"
set "OUTNAME=GlimRegionViewer_v%VERSION%"
set "OUT=%ROOT%\deploy\%OUTNAME%"

REM OpenCV runtime dll location (override by passing arg1 = opencv bin dir)
if not "%~1"=="" (
    set "OPENCV_BIN=%~1"
) else (
    set "OPENCV_BIN=C:\Lib\opencv\build\x64\vc16\bin"
)

echo [make_deploy] Version : %VERSION%
echo [make_deploy] Root    : %ROOT%
echo [make_deploy] Source  : %SRC%
echo [make_deploy] Output  : %OUT%
echo.

if not exist "%SRC%\GlimRegionViewer.exe" (
    echo [ERROR] Release exe not found:
    echo         %SRC%\GlimRegionViewer.exe
    echo         Build Release^|x64 first, then re-run.
    exit /b 1
)

REM Fresh output dir
if exist "%OUT%" rmdir /s /q "%OUT%"
mkdir "%OUT%" 2>nul

REM 1) exe
copy /y "%SRC%\GlimRegionViewer.exe" "%OUT%\" >nul
if errorlevel 1 ( echo [ERROR] copy exe failed & exit /b 1 )

REM 2) opencv_world4120.dll  (from Release dir first, else OPENCV_BIN)
if exist "%SRC%\opencv_world4120.dll" (
    copy /y "%SRC%\opencv_world4120.dll" "%OUT%\" >nul
) else if exist "%OPENCV_BIN%\opencv_world4120.dll" (
    copy /y "%OPENCV_BIN%\opencv_world4120.dll" "%OUT%\" >nul
) else (
    echo [WARN] opencv_world4120.dll not found in:
    echo        %SRC%
    echo        %OPENCV_BIN%
    echo        Copy it into "%OUT%" manually before shipping.
)

REM 3) profiles\*.ini
if exist "%ROOT%\profiles" (
    mkdir "%OUT%\profiles" 2>nul
    copy /y "%ROOT%\profiles\*.ini" "%OUT%\profiles\" >nul
)

REM 4) README.txt (Korean usage guide)
if exist "%SCRIPT_DIR%DEPLOY_README.txt" (
    copy /y "%SCRIPT_DIR%DEPLOY_README.txt" "%OUT%\README.txt" >nul
)

echo.
echo [make_deploy] Collected files:
dir /b "%OUT%"
echo.

REM 5) zip via PowerShell Compress-Archive
set "ZIP=%ROOT%\deploy\%OUTNAME%.zip"
if exist "%ZIP%" del /q "%ZIP%"
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "Compress-Archive -Path '%OUT%\*' -DestinationPath '%ZIP%' -Force"
if errorlevel 1 (
    echo [WARN] zip step failed. Folder is ready at: %OUT%
) else (
    echo [make_deploy] Zip created: %ZIP%
)

echo.
echo [make_deploy] Done.
endlocal
