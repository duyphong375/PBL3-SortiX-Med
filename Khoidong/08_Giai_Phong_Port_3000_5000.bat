@echo off
chcp 65001 >nul
title SortiX-Med - Giai Phong Cong Mang 3000, 5000 va 8000
setlocal
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DASHBOARD_DIR=%ROOT_DIR%\SortiX-Dashboard"

if not exist "%DASHBOARD_DIR%\scripts\free-port.cjs" (
    echo [LỖI] Không tìm thấy "%DASHBOARD_DIR%\scripts\free-port.cjs"
    pause
    exit /b 1
)

echo =====================================================================
echo    SORTIX-MED: GIẢI PHÓNG CỔNG MẠNG 3000, 5000 VÀ 8000 (AI CAM)
echo =====================================================================
echo.
echo Đang quét và tắt các tiến trình đang chiếm dụng cổng 3000, 5000, 8000...
cd /d "%DASHBOARD_DIR%"
node scripts/free-port.cjs 3000
node scripts/free-port.cjs 5000
node scripts/free-port.cjs 8000

echo.
echo [THÀNH CÔNG] Ba cổng 3000, 5000 và 8000 đã sẵn sàng để khởi chạy mới.
echo =====================================================================
pause
