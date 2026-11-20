@echo off
chcp 65001 >nul
title SortiX-Med - Chi Chay Backend Server (Port 5000)
setlocal
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DASHBOARD_DIR=%ROOT_DIR%\SortiX-Dashboard"

if not exist "%DASHBOARD_DIR%\package.json" (
    echo [LỖI] Không tìm thấy thư mục Dashboard tại: "%DASHBOARD_DIR%"
    pause
    exit /b 1
)

echo =====================================================================
echo    SORTIX-MED: CHỈ CHẠY MÁY CHỦ BACKEND NODE.JS NATIVE (CỔNG 5000)
echo =====================================================================
echo.
echo Đang giải phóng cổng 5000 nếu bị kẹt...
cd /d "%DASHBOARD_DIR%"
node scripts/free-port.cjs 5000

echo Đang khởi chạy máy chủ backend...
echo Endpoint REST API và Telemetry: http://localhost:5000
echo.

call npm run dev:backend
set "EXIT_CODE=%ERRORLEVEL%"
if %EXIT_CODE% neq 0 (
    echo.
    echo [LỖI] Backend kết thúc với mã lỗi %EXIT_CODE%.
)

pause
exit /b %EXIT_CODE%
