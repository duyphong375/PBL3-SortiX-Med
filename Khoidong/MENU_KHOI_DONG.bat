@echo off
chcp 65001 >nul
title SortiX-Med - Menu Khoi Dong He Thong
setlocal enabledelayedexpansion
set "SCRIPT_DIR=%~dp0"
for %%I in ("%~dp0..") do set "ROOT_DIR=%%~fI"
set "DASHBOARD_DIR=%ROOT_DIR%\SortiX-Dashboard"
set "FIRMWARE_DIR=%ROOT_DIR%\sortix-firmware"

:MENU
cls
echo ===============================================================================
echo                HỆ THỐNG PHÂN LOẠI DỤNG CỤ Y TẾ SORTIX-MED
echo                        TRUNG TÂM KHỞI ĐỘNG HỆ THỐNG
echo ===============================================================================
echo.
echo   --- [A. PHÂN HỆ DASHBOARD, AI CAMERA VA GIÁM SÁT SCADA] ---
echo     [1] Khởi động TOÀN BỘ (Frontend :3000 + Backend :5000 + Telegram + AI Cam :8000)
echo     [2] Chỉ chạy giao diện Frontend Next.js (:3000)
echo     [3] Chỉ chạy máy chủ Backend Node.js (:5000)
echo     [4] Chỉ chạy Telegram Bot Gateway Poller (@SortiX_Med_Bot)
echo     [10] Chỉ chạy AI Camera Server YOLO11 (:8000)
echo.
echo   --- [B. PHÂN HỆ NHÚNG FIRMWARE ESP32-C5] ---
echo     [5] Biên dịch Firmware ESP32-C5 (Build qua ESP-IDF v5.5.4)
echo     [6] Nạp Code và Mở Serial Monitor ESP32-C5 (Flash and Monitor)
echo     [7] Mở Serial Monitor ESP32-C5 (Chỉ xem log gỡ lỗi, không nạp lại)
echo.
echo   --- [C. TIỆN ÍCH VA VẬN HÀNH ĐỒNG THỜI] ---
echo     [8] Giải phóng cổng mạng (Free Port 3000, 5000 và 8000 nếu bị kẹt)
echo     [9] Khởi động ĐỒNG THỜI (Dashboard + AI Camera + Nạp/Monitor ESP32-C5)
echo     [0] Thoát chương trình
echo.
echo ===============================================================================
set "CHOICE="
set /p CHOICE="Vui lòng chọn chức năng [0-10]: "
if defined CHOICE set "CHOICE=%CHOICE: =%"

if "%CHOICE%"=="1" goto OP_DASHBOARD_ALL
if "%CHOICE%"=="2" goto OP_FRONTEND
if "%CHOICE%"=="3" goto OP_BACKEND
if "%CHOICE%"=="4" goto OP_TELEGRAM
if "%CHOICE%"=="5" goto OP_BUILD_FW
if "%CHOICE%"=="6" goto OP_FLASH_MONITOR
if "%CHOICE%"=="7" goto OP_MONITOR
if "%CHOICE%"=="8" goto OP_FREE_PORT
if "%CHOICE%"=="9" goto OP_START_DUAL
if "%CHOICE%"=="10" goto OP_AI_CAMERA
if "%CHOICE%"=="0" goto EXIT_PROG

echo.
echo Lựa chọn không hợp lệ, vui lòng chọn từ 0 đến 10.
timeout /t 2 >nul
goto MENU

:OP_DASHBOARD_ALL
start "SortiX Dashboard + AI Cam" "%SCRIPT_DIR%01_Khoi_Dong_Tat_Ca_Dashboard.bat"
goto MENU

:OP_FRONTEND
start "SortiX Frontend 3000" "%SCRIPT_DIR%02_Chi_Chay_Frontend_3000.bat"
goto MENU

:OP_BACKEND
start "SortiX Backend 5000" "%SCRIPT_DIR%03_Chi_Chay_Backend_5000.bat"
goto MENU

:OP_TELEGRAM
start "SortiX Telegram Bot" "%SCRIPT_DIR%04_Chi_Chay_Telegram_Bot.bat"
goto MENU

:OP_AI_CAMERA
start "SortiX AI Camera 8000" "%SCRIPT_DIR%09_Chi_Chay_AI_Camera_8000.bat"
goto MENU

:OP_BUILD_FW
start "SortiX Firmware Build" "%SCRIPT_DIR%05_Build_Firmware_ESP32C5.bat"
goto MENU

:OP_FLASH_MONITOR
start "SortiX Flash and Monitor" "%SCRIPT_DIR%06_Nap_Code_Va_Monitor_ESP32C5.bat"
goto MENU

:OP_MONITOR
start "SortiX Serial Monitor" "%SCRIPT_DIR%07_Mo_Serial_Monitor_ESP32C5.bat"
goto MENU

:OP_FREE_PORT
call "%SCRIPT_DIR%08_Giai_Phong_Port_3000_5000.bat"
goto MENU

:OP_START_DUAL
echo.
echo Đang khởi động đồng thời Dashboard + AI Camera và cửa sổ Nạp/Monitor ESP32-C5...
start "SortiX Dashboard + AI Cam" "%SCRIPT_DIR%01_Khoi_Dong_Tat_Ca_Dashboard.bat"
timeout /t 2 >nul
start "SortiX Flash and Monitor" "%SCRIPT_DIR%06_Nap_Code_Va_Monitor_ESP32C5.bat"
goto MENU

:EXIT_PROG
echo.
echo Tạm biệt! Cảm ơn bạn đã sử dụng SortiX-Med.
timeout /t 1 >nul
exit /b 0
