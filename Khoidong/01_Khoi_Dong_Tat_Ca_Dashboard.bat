@echo off
chcp 65001 >nul
title SortiX-Med - Khoi Dong Toan Bo He Thong (Dashboard + AI Camera)
setlocal
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DASHBOARD_DIR=%ROOT_DIR%\SortiX-Dashboard"
set "AI_SERVER_DIR=%DASHBOARD_DIR%\ai-server"

if not exist "%DASHBOARD_DIR%\package.json" (
    echo [LỖI] Không tìm thấy thư mục Dashboard tại: "%DASHBOARD_DIR%"
    pause
    exit /b 1
)

echo =====================================================================
echo    SORTIX-MED: KHỞI ĐỘNG TOÀN BỘ DASHBOARD ^& AI CAMERA
echo =====================================================================
echo.
echo [1/4] Đang kiểm tra và giải phóng các cổng mạng (3000, 5000, 8000)...
cd /d "%DASHBOARD_DIR%"
node scripts/free-port.cjs 3000
node scripts/free-port.cjs 5000
node scripts/free-port.cjs 8000

echo [2/4] Đang khởi chạy AI Camera Server (:8000)...
if exist "%AI_SERVER_DIR%\run_camera.bat" (
    start "SortiX AI Camera 8000" /d "%AI_SERVER_DIR%" "%AI_SERVER_DIR%\run_camera.bat"
) else (
    echo [CẢNH BÁO] Không tìm thấy "%AI_SERVER_DIR%\run_camera.bat", bỏ qua AI Camera.
)

echo [3/4] Đang khởi chạy Frontend (:3000), Backend (:5000) và Telegram Bot...
echo.
echo  * Frontend : http://localhost:3000
echo  * Backend  : http://localhost:5000
echo  * AI Camera: http://localhost:8000/video_feed
echo  * Telegram : Đang kích hoạt Poller 2 chiều (@SortiX_Med_Bot)
echo.
echo ---------------------------------------------------------------------
echo Nhấn Ctrl + C để dừng Frontend/Backend/Telegram (đóng riêng cửa sổ AI Camera nếu cần).
echo ---------------------------------------------------------------------
echo.

echo [4/4] Trình duyệt sẽ tự mở sau 3 giây...
start "" /min powershell -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Seconds 3; Start-Process 'http://localhost:3000'"

call npm run dev:all
set "EXIT_CODE=%ERRORLEVEL%"
if %EXIT_CODE% neq 0 (
    echo.
    echo [LỖI] Hệ thống Dashboard kết thúc với mã lỗi %EXIT_CODE%.
)

pause
exit /b %EXIT_CODE%
