@echo off
chcp 65001 >nul
title SortiX-Med - Chi Chay Frontend (Port 3000)
setlocal
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DASHBOARD_DIR=%ROOT_DIR%\SortiX-Dashboard"

if not exist "%DASHBOARD_DIR%\package.json" (
    echo [LỖI] Không tìm thấy thư mục Dashboard tại: "%DASHBOARD_DIR%"
    pause
    exit /b 1
)

echo =====================================================================
echo    SORTIX-MED: CHỈ CHẠY GIAO DIỆN FRONTEND NEXT.JS (CỔNG 3000)
echo =====================================================================
echo.
echo Đang giải phóng cổng 3000 nếu bị kẹt...
cd /d "%DASHBOARD_DIR%"
node scripts/free-port.cjs 3000

echo Đang khởi chạy máy chủ phát triển Next.js 14...
echo Truy cập: http://localhost:3000
echo.

start "" /min powershell -NoProfile -WindowStyle Hidden -Command "Start-Sleep -Seconds 3; Start-Process 'http://localhost:3000'"

call npm run dev:frontend
set "EXIT_CODE=%ERRORLEVEL%"
if %EXIT_CODE% neq 0 (
    echo.
    echo [LỖI] Frontend kết thúc với mã lỗi %EXIT_CODE%.
)

pause
exit /b %EXIT_CODE%
