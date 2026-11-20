@echo off
chcp 65001 >nul
title SortiX-Med - Bien Dich Firmware ESP32-C5 (Build)
setlocal
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "FIRMWARE_DIR=%ROOT_DIR%\sortix-firmware"
set "IDF_PROFILE=C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1"

echo =====================================================================
echo    SORTIX-MED: BIÊN DỊCH FIRMWARE ESP32-C5 (ESP-IDF v5.5.4)
echo =====================================================================
echo.
if not exist "%FIRMWARE_DIR%\CMakeLists.txt" (
    echo [LỖI] Không tìm thấy project firmware tại: "%FIRMWARE_DIR%"
    pause
    exit /b 1
)

if not exist "%IDF_PROFILE%" (
    echo [LỖI] Không tìm thấy file kích hoạt môi trường ESP-IDF tại:
    echo        %IDF_PROFILE%
    echo Vui lòng kiểm tra lại đường dẫn cài đặt Espressif trên máy tính.
    echo.
    pause
    exit /b 1
)

echo [1/2] Đang kích hoạt môi trường ESP-IDF v5.5.4...
echo [2/2] Bắt đầu kiểm tra target và biên dịch tại: "%FIRMWARE_DIR%"
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$ErrorActionPreference = 'Stop';" ^
    ". '%IDF_PROFILE%';" ^
    "Set-Location '%FIRMWARE_DIR%';" ^
    "$needTarget = $true;" ^
    "if (Test-Path 'sdkconfig') {" ^
    "  if (Select-String -Path 'sdkconfig' -Pattern 'CONFIG_IDF_TARGET=\"esp32c5\"' -SimpleMatch -Quiet) { $needTarget = $false }" ^
    "}" ^
    "if ($needTarget) {" ^
    "  Write-Host 'Đang thiết lập target esp32c5...' -ForegroundColor Cyan;" ^
    "  idf.py set-target esp32c5;" ^
    "  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }" ^
    "} else {" ^
    "  Write-Host 'Target đã là esp32c5 (sdkconfig), bỏ qua set-target.' -ForegroundColor DarkCyan;" ^
    "}" ^
    "Write-Host 'Bắt đầu biên dịch (idf.py build)...' -ForegroundColor Green;" ^
    "idf.py build;" ^
    "exit $LASTEXITCODE"

set "EXIT_CODE=%ERRORLEVEL%"
echo.
echo =====================================================================
if %EXIT_CODE% equ 0 (
    echo [THÀNH CÔNG] Biên dịch firmware ESP32-C5 hoàn tất!
) else (
    echo [LỖI] Biên dịch thất bại với mã lỗi %EXIT_CODE%. Xem chi tiết bên trên.
)
echo =====================================================================
pause
exit /b %EXIT_CODE%
