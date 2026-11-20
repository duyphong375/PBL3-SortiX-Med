@echo off
chcp 65001 >nul
title SortiX-Med - Chi Chay AI Camera Server (Port 8000)
setlocal
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DASHBOARD_DIR=%ROOT_DIR%\SortiX-Dashboard"
set "AI_SERVER_DIR=%DASHBOARD_DIR%\ai-server"
set "DEFAULT_CAM_URL=http://192.168.137.155:4747/video"

if not exist "%AI_SERVER_DIR%\server.py" (
    echo [LỖI] Không tìm thấy AI Camera Server tại: "%AI_SERVER_DIR%\server.py"
    pause
    exit /b 1
)

echo =====================================================================
echo    SORTIX-MED: KHỞI ĐỘNG AI CAMERA SERVER (YOLO11 + STREAM :8000)
echo =====================================================================
echo.
echo Gợi ý nguồn Camera (iPhone DroidCam):
echo  - Nhấn Enter ngay      : Dùng iPhone mặc định (%DEFAULT_CAM_URL%)
echo  - Nhập 0 hoặc 1        : Dùng Webcam laptop (0) hoặc DroidCam USB (1)
echo  - Nhập URL Wi-Fi khác  : Ví dụ http://192.168.137.x:4747/video
echo.

set "CAM_SRC="
if not "%~1"=="" set "CAM_SRC=%~1"
if "%CAM_SRC%"=="" (
    set /p CAM_SRC="Nhập nguồn Camera (Enter để dùng %DEFAULT_CAM_URL%): "
)
if "%CAM_SRC%"=="" set "CAM_SRC=%DEFAULT_CAM_URL%"

echo.
echo Đang giải phóng cổng 8000 nếu bị kẹt...
cd /d "%DASHBOARD_DIR%"
if exist "scripts\free-port.cjs" node scripts/free-port.cjs 8000

cd /d "%AI_SERVER_DIR%"
set "NO_PAUSE=1"
call "%AI_SERVER_DIR%\run_camera.bat" "%CAM_SRC%"
set "EXIT_CODE=%ERRORLEVEL%"

echo.
pause
exit /b %EXIT_CODE%
