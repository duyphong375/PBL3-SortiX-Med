@echo off
chcp 65001 >nul
title SortiX-Med - Nap Firmware ESP32-C5 (esptool)
setlocal
cd /d "%~dp0"

set "PY_CMD=python"
if exist "C:\Espressif\tools\python\v5.5.4\venv\Scripts\python.exe" (
    set "PY_CMD=C:\Espressif\tools\python\v5.5.4\venv\Scripts\python.exe"
)

echo ============================================================
echo   SortiX Conveyor Firmware - Nạp Firmware WeAct ESP32-C5
echo ============================================================
echo.

if not exist "build\sortix-firmware.bin" (
    echo [LỖI] Chưa thấy file "build\sortix-firmware.bin".
    echo Hãy build firmware trước bằng ESP-IDF hoặc 05_Build_Firmware_ESP32C5.bat!
    pause
    exit /b 1
)

echo [DANH SÁCH CÁC CỔNG COM ĐANG KẾT NỐI]:
powershell -NoProfile -Command ^
    "$ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object;" ^
    "if ($ports.Count -eq 0) { Write-Host '  (Chưa phát hiện cổng COM nào)' -ForegroundColor Yellow } else {" ^
    "  foreach ($p in $ports) { Write-Host \"  -> $p\" -ForegroundColor Green }" ^
    "}"
echo.

set "COMPORT="
set /p COMPORT="Nhập cổng COM của ESP32-C5 (ví dụ COM12 hoặc 12, Enter để dùng COM12): "
set "COMPORT=%COMPORT: =%"
if "%COMPORT%"=="" set "COMPORT=COM12"
echo %COMPORT%| findstr /r "^[0-9][0-9]*$" >nul && set "COMPORT=COM%COMPORT%"

echo.
echo Đang nạp firmware vào cổng %COMPORT%...
"%PY_CMD%" -m esptool --chip esp32c5 -p "%COMPORT%" -b 460800 --before default_reset --after hard_reset --no-stub write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m 0x2000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0xf000 build\ota_data_initial.bin 0x20000 build\sortix-firmware.bin
set "EXIT_CODE=%ERRORLEVEL%"

if %EXIT_CODE% equ 0 (
    echo.
    echo ============================================================
    echo [THÀNH CÔNG] Firmware đã được nạp hoàn tất vào ESP32-C5!
    echo ============================================================
    echo.
    echo HƯỚNG DẪN KẾT NỐI WI-FI TẠI NHÀ BẠN:
    echo 1. Sau khi nạp xong, nhấn nút RST ^(hoặc cắm lại cáp USB^) để khởi động.
    echo 2. Chờ 10 giây, ESP32-C5 sẽ tự động phát mạng Wi-Fi:
    echo      - Tên Wi-Fi ^(SSID^): SortiX-Setup
    echo      - Mật khẩu:         12345678
    echo 3. Dùng điện thoại hoặc laptop kết nối vào mạng 'SortiX-Setup'.
    echo 4. Trình duyệt sẽ tự mở ^(hoặc tự vào địa chỉ http://192.168.4.1^).
    echo 5. Nhập tên Wi-Fi và Mật khẩu Wi-Fi nhà bạn rồi nhấn 'Save ^& Restart'.
    echo 6. ESP32-C5 sẽ lưu vĩnh viễn vào bộ nhớ flash và tự động kết nối Internet!
    echo.
) else (
    echo.
    echo ============================================================
    echo [LỖI] Quá trình nạp chưa thành công ^(mã lỗi %EXIT_CODE%^)!
    echo ============================================================
    echo Hãy kiểm tra các bước sau:
    echo 1. Kiểm tra đúng cổng COM trong Device Manager ^(Ports COM ^& LPT^).
    echo 2. Đảm bảo đã cài driver USB UART CH343 hoặc USB-JTAG cho bo WeAct.
    echo 3. Thử giữ nút BOOT trên bo mạch, ấn nhả RST rồi nhấn nạp lại.
    echo 4. Nếu đường truyền không ổn định, thử hạ baud rate từ 460800 xuống 115200.
    echo.
)
pause
exit /b %EXIT_CODE%
