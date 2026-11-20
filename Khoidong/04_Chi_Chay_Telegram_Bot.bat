@echo off
chcp 65001 >nul
title SortiX-Med - Chi Chay Telegram Bot Poller (@SortiX_Med_Bot)
setlocal
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DASHBOARD_DIR=%ROOT_DIR%\SortiX-Dashboard"

if not exist "%DASHBOARD_DIR%\package.json" (
    echo [LỖI] Không tìm thấy thư mục Dashboard tại: "%DASHBOARD_DIR%"
    pause
    exit /b 1
)

echo =====================================================================
echo    SORTIX-MED: CHẠY TELEGRAM BOT GATEWAY 2 CHIỀU (@SortiX_Med_Bot)
echo =====================================================================
echo.
echo Khởi chạy Poller lắng nghe lệnh từ Telegram:
echo  * Lệnh tra cứu: /status, /temp, /help
echo  * Lệnh điều khiển SCADA: /start, /stop, /estop, /clear_bin
echo.
cd /d "%DASHBOARD_DIR%"
call npm run dev:telegram
set "EXIT_CODE=%ERRORLEVEL%"
if %EXIT_CODE% neq 0 (
    echo.
    echo [LỖI] Telegram Bot kết thúc với mã lỗi %EXIT_CODE%.
)

pause
exit /b %EXIT_CODE%
