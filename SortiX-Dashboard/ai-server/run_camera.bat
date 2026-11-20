@echo off
chcp 65001 >nul
title SortiX-Med - AI Camera Server (Port 8000)
setlocal
cd /d "%~dp0"

set "FREE_PORT_SCRIPT=%~dp0..\scripts\free-port.cjs"
if exist "%FREE_PORT_SCRIPT%" (
    where node >nul 2>nul
    if %ERRORLEVEL% equ 0 (
        node "%FREE_PORT_SCRIPT%" 8000
    )
)

set "DEFAULT_CAM_URL=http://192.168.137.231:4747/video"
if "%~1"=="" if not defined CAMERA_SOURCE set "CAMERA_SOURCE=%DEFAULT_CAM_URL%"
if not defined CAMERA_ROTATE set "CAMERA_ROTATE=90"
set "DISPLAY_CAM=%CAMERA_SOURCE%"
if not "%~1"=="" set "DISPLAY_CAM=%~1"

echo ========================================================
echo   KHỞI ĐỘNG SORTIX-MED AI CAMERA SERVER (YOLO + MQTT)
echo ========================================================
echo   Stream MJPEG : http://localhost:8000/video_feed
echo   Health Check : http://localhost:8000/health
echo   Camera iPhone: %DISPLAY_CAM% (xoay %CAMERA_ROTATE% độ)
echo   Nhấn Ctrl + C để dừng AI Camera Server.
echo ========================================================
echo.

if exist ".venv\Scripts\python.exe" (
    ".venv\Scripts\python.exe" -u server.py %*
) else (
    python -u server.py %*
)

set "EXIT_CODE=%ERRORLEVEL%"
if %EXIT_CODE% neq 0 (
    echo.
    echo [LỖI] AI Camera Server kết thúc với mã lỗi %EXIT_CODE%.
)

if not defined NO_PAUSE pause
exit /b %EXIT_CODE%
