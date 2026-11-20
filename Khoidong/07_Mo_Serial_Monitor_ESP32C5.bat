@echo off
chcp 65001 >nul
title SortiX-Med - Mo Serial Monitor ESP32-C5 (Khong Nap Lai)
setlocal
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "FIRMWARE_DIR=%ROOT_DIR%\sortix-firmware"
set "IDF_PROFILE=C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1"

echo =====================================================================
echo    SORTIX-MED: MỞ SERIAL MONITOR ESP32-C5 (XEM LOG THỜI GIAN THỰC)
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
    echo.
    pause
    exit /b 1
)

echo [DANH SÁCH CÁC CỔNG COM ĐANG KẾT NỐI VÀO MÁY TÍNH]:
powershell -NoProfile -Command ^
    "$ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object;" ^
    "if ($ports.Count -eq 0) { Write-Host '  (Chưa phát hiện cổng COM nào. Hãy cắm cáp USB vào ESP32-C5)' -ForegroundColor Yellow } else {" ^
    "  foreach ($p in $ports) { Write-Host \"  -> $p\" -ForegroundColor Green }" ^
    "}"
echo.

set "COMPORT="
set /p COMPORT="Nhập cổng COM của mạch (ví dụ COM12 hoặc 12, Enter để dùng COM12): "
set "COMPORT=%COMPORT: =%"
if "%COMPORT%"=="" set "COMPORT=COM12"
echo %COMPORT%| findstr /r "^[0-9][0-9]*$" >nul && set "COMPORT=COM%COMPORT%"

echo.
echo ---------------------------------------------------------------------
echo Đang mở Serial Monitor trên cổng %COMPORT%...
echo Nhấn Ctrl + ] để thoát khỏi Monitor.
echo ---------------------------------------------------------------------
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    ". '%IDF_PROFILE%';" ^
    "Set-Location '%FIRMWARE_DIR%';" ^
    "idf.py -p '%COMPORT%' monitor;" ^
    "exit $LASTEXITCODE"

set "EXIT_CODE=%ERRORLEVEL%"
if %EXIT_CODE% neq 0 (
    echo.
    echo [LỖI] Serial Monitor kết thúc với mã lỗi %EXIT_CODE%.
)

pause
exit /b %EXIT_CODE%
