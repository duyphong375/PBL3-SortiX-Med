# SortiX Firmware (ESP-IDF v5.5.x)
### Vi điều khiển: ESP32-C5 RISC-V 32-bit Single-Core @ 240 MHz (WeAct Studio ESP32-C5-A-N8R8)

---

## 1. Giới Thiệu
Mã nguồn firmware điều khiển hệ thống băng tải phân loại dụng cụ y tế tự động SortiX. Firmware kết nối với hệ thống AI thị giác máy tính (YOLOv8 chạy trên PC) và ứng dụng điều khiển trung tâm **SortiX-Dashboard** thông qua giao thức **MQTT** và **HTTP REST**.

---

## 2. Toàn Bộ Cấu Trúc Thư Mục & Vai Trò Từng Thành Phần

```text
sortix-firmware/
├── CMakeLists.txt                          # CMake project gốc, kết nối toolchain ESP-IDF
├── sdkconfig.defaults                      # Cấu hình cứng target esp32c5, Flash 8MB, PSRAM 8MB
├── partitions.csv                          # Bảng phân vùng Flash 8MB (NVS, Otadata, OTA_0, OTA_1, Storage)
├── README.md                               # Sổ tay kiến trúc, pinout, hướng dẫn build firmware
│
├── main/                                   # THƯ MỤC CHA 1: Ứng dụng chính & Định nghĩa lõi
│   ├── CMakeLists.txt                      # Đăng ký main.c và khai báo các component phụ thuộc
│   ├── idf_component.yml                   # Khai báo dependency cJSON (espressif/cjson)
│   ├── Kconfig.projbuild                   # Cấu hình menu tương tác menuconfig (Wi-Fi, MQTT, timeout)
│   ├── main.c                              # Khởi tạo app_main(), các FreeRTOS Tasks & đăng ký Callbacks
│   ├── app_config.h                        # Macro cấu hình toàn hệ thống, MQTT topics, HTTP endpoint
│   ├── pin_definitions.h                   # Sơ đồ gán chân GPIO chuẩn hóa theo BOM danh-sach-linh-kien.md
│   └── version.h                           # Định nghĩa thông tin phiên bản firmware v1.0.0
│
├── components/                             # THƯ MỤC CHA 2: Các module chức năng độc lập (14 components)
│   ├── motor_control/                      # Điều khiển động cơ DC qua L298N (IN1: GPIO8, IN2: GPIO9)
│   │   ├── include/motor_control.h         # Header: forward, reverse, dynamic brake, coast/stop
│   │   ├── motor_control.c                 # Code thực thi: Mutex bảo vệ trạng thái, ghi mức GPIO
│   │   └── CMakeLists.txt
│   │
│   ├── encoder/                            # Đọc encoder 2 pha dùng New PCNT Driver (driver/pulse_cnt.h)
│   │   ├── include/encoder.h               # Header: đọc count 64-bit, reset, quy đổi counts <-> mm
│   │   ├── encoder.c                       # Code thực thi: Quadrature x4, bộ lọc nhiễu 1us, watchpoint overflow
│   │   └── CMakeLists.txt
│   │
│   ├── servo/                              # Điều khiển 2 servo MG996R non-blocking qua LEDC PWM (GPIO6, GPIO24)
│   │   ├── include/servo.h                 # Header: trigger non-blocking, reset, đọc FSM state
│   │   ├── servo.c                         # Code thực thi: FSM (IDLE->OPENING->HOLDING->CLOSING->IDLE) qua esp_timer
│   │   └── CMakeLists.txt
│   │
│   ├── sensors/                            # Quản lý cảm biến quang và cảm biến ánh sáng
│   │   ├── include/sensors.h               # Header: ngắt S1 chốt xung encoder và timestamp, đọc LDR
│   │   ├── sensors.c                       # Code thực thi: Ngắt cạnh xuống GPIO0, debounce 80ms, đọc DO GPIO5
│   │   └── CMakeLists.txt
│   │
│   ├── button/                             # Nút nhấn Start vật lý (GPIO23)
│   │   ├── include/button.h                # Header: callback khi nhấn nút Start
│   │   ├── button.c                        # Code thực thi: Ngắt cạnh xuống GPIO23 kèm timer debounce 20ms
│   │   └── CMakeLists.txt
│   │
│   ├── led_indicator/                      # Đèn LED trạng thái màu xanh lá (GPIO10)
│   │   ├── include/led_indicator.h         # Header: chế độ OFF, IDLE (1Hz), RUNNING (ON), JAMMED (5Hz)
│   │   ├── led_indicator.c                 # Code thực thi: Quản lý nhấp nháy qua software timer 100ms
│   │   └── CMakeLists.txt
│   │
│   ├── wifi_manager/                       # Quản lý kết nối Wi-Fi Station & SoftAP Fallback
│   │   ├── include/wifi_manager.h          # Header: kiểm tra trạng thái, đọc RSSI, lấy chuỗi IP và Band
│   │   ├── wifi_manager.c                  # Code thực thi: Tự động kết nối lại, nhận diện 2.4GHz / 5GHz Wi-Fi 6
│   │   └── CMakeLists.txt
│   │
│   ├── mqtt_handler/                       # MQTT Client đồng bộ 100% schemas & topics SortiX-Dashboard
│   │   ├── include/mqtt_handler.h          # Header: publish telemetry, heartbeat, classification, jam, temp
│   │   ├── mqtt_handler.c                  # Code thực thi: Subscribe vision, control, config/set, backoff reconnect
│   │   └── CMakeLists.txt
│   │
│   ├── http_telemetry/                     # Gửi HTTP POST telemetry lên backend Node.js port 5000
│   │   ├── include/http_telemetry.h        # Header: gửi payload định kỳ 5s
│   │   ├── http_telemetry.c                # Code thực thi: Đóng gói JSON (tuyệt đối không gửi optical_sensor)
│   │   └── CMakeLists.txt
│   │
│   ├── sorting_engine/                     # Trọng tâm điều khiển logic phân loại
│   │   ├── include/sorting_engine.h        # Header: queue 15 phôi, matching FIFO, apply config động
│   │   ├── sorting_engine.c                # Code thực thi: Tính cự ly encoder, kích hoạt servo, fallback timeout
│   │   └── CMakeLists.txt
│   │
│   ├── state_machine/                      # Quản lý trạng thái hệ thống
│   │   ├── include/state_machine.h         # Header: SYSTEM_STATE_IDLE, RUNNING, STOPPED, JAMMED
│   │   ├── state_machine.c                 # Code thực thi: Chuyển trạng thái, khóa an toàn E-Stop, an toàn boot
│   │   └── CMakeLists.txt
│   │
│   ├── safety_monitor/                     # Giám sát an toàn kẹt cơ khí & nhiệt độ SoC
│   │   ├── include/safety_monitor.h        # Header: callback kẹt phôi, callback cảnh báo nhiệt
│   │   ├── safety_monitor.c                # Code thực thi: Encoder stall detection (>1.8s) & driver/temperature_sensor.h
│   │   └── CMakeLists.txt
│   │
│   ├── nvs_config/                         # Lưu trữ cấu hình bền vững vào NVS Flash
│   │   ├── include/nvs_config.h            # Header: struct sys_config, load, save, factory reset
│   │   ├── include/app_config.h            # Header dùng chung các topics và tham số hệ thống
│   │   ├── nvs_config.c                    # Code thực thi: Ghi đọc NVS namespace sortix_cfg
│   │   └── CMakeLists.txt
│   │
│   └── captive_portal/                     # Cấu hình Wi-Fi/MQTT qua trang Web SoftAP nội bộ
│       ├── include/captive_portal.h        # Header: start, stop HTTP server port 80
│       ├── captive_portal.c                # Code thực thi: Giao diện web HTML, nhận form POST và lưu NVS
│       └── CMakeLists.txt
│
└── test/                                   # THƯ MỤC CHA 3: Unit Tests với Unity Test Framework
    ├── CMakeLists.txt                      # Cấu hình biên dịch runner test
    ├── test_encoder.c                      # Kiểm thử quy đổi xung - mm và xử lý overflow
    ├── test_sorting_engine.c               # Kiểm thử ring buffer FIFO, vision matching, dynamic config
    └── test_state_machine.c                # Kiểm thử an toàn khởi động IDLE, E-Stop release
```

---

## 3. Bảng Phân Bổ GPIO Chuẩn (Đối chiếu BOM danh-sach-linh-kien.md)

| Chức năng                   | GPIO     | Vị trí Header WeAct | Chế độ / Cấu hình                 |
|-----------------------------|----------|---------------------|-----------------------------------|
| Cảm biến vật cản IR (S1)    | **GPIO0**  | Trái Pin 5          | Input, Pull-up, Falling Edge ISR |
| Encoder Kênh A (Xung)       | **GPIO1**  | Trái Pin 6          | PCNT Pulse Counter (Edge Input)  |
| Encoder Kênh B (Chiều)      | **GPIO4**  | Phải Pin 8          | PCNT Pulse Counter (Level Input) |
| Cảm biến quang trở LDR (DO) | **GPIO5**  | Phải Pin 9          | Input Digital                     |
| Servo 1 - PWM (Gạt 1)      | **GPIO6**  | Trái Pin 7          | LEDC PWM 50Hz (Timer 0, Ch 0)    |
| Driver L298N IN1            | **GPIO8**  | Trái Pin 9          | Output (FWD=1, REV=0, BRAKE=1)    |
| Driver L298N IN2            | **GPIO9**  | Trái Pin 10         | Output (FWD=0, REV=1, BRAKE=1)    |
| LED trạng thái (Xanh lá)    | **GPIO10** | Trái Pin 11         | Output, 100ms Software Timer      |
| Nút Start vật lý            | **GPIO23** | Phải Pin 5          | Input Pull-up, Debounce RC + 20ms |
| Servo 2 - PWM (Gạt 2)      | **GPIO24** | Phải Pin 4          | LEDC PWM 50Hz (Timer 0, Ch 1)    |

### ⚠ CÁC CHÂN CẤM SỬ DỤNG (FORBIDDEN GPIOs):
- **GPIO15 (SPICS1):** Đã nối cứng nội bộ với IC PSRAM 8MB Quad SPI bên trong nắp shield kim loại.
- **GPIO13 / GPIO14:** Cổng USB Native (D- / D+) dùng nạp code, JTAG và USB-CDC Debug.
- **GPIO11 / GPIO12:** Cổng UART0 Console (TX0 / RX0 qua chip CH343).
- **GPIO26 / GPIO27 / GPIO28:** Chân cấu hình khởi động (Strapping Pins), RGB LED và nút BOOT onboard.

---

## 4. Kiến Trúc Nhiệm Vụ FreeRTOS & An Toàn

1. **`task_conveyor` (Priority 6):**
   - Quản lý điều khiển cầu H L298N (Tiến, Lùi, Thắng động cơ, Thả trôi).
   - Đọc bộ đếm xung phần cứng từ New PCNT Driver (`driver/pulse_cnt.h`).
   - Giám sát hiện tượng kẹt cơ khí (Encoder Stall): Khi động cơ đang chạy, nếu xung encoder không thay đổi quá 1.8s (ngoài thời gian ân hạn khởi động 1.0s), hệ thống kích hoạt phanh động cơ ngay lập tức và chuyển sang trạng thái `JAMMED`.
2. **`task_sorting` (Priority 5):**
   - Quản lý bộ đệm xoay vòng FIFO (Tracking Queue) tối đa 15 vật thể trên băng tải.
   - Nhận ngắt từ S1 (GPIO0) kèm giá trị xung encoder và timestamp vi giây.
   - Ghép cặp kết quả Vision AI từ MQTT (`sorter/sorter_01/vision`) theo thứ tự FIFO.
   - Điều khiển Servo 1 (GPIO6) và Servo 2 (GPIO24) qua FSM Non-blocking (`IDLE -> OPENING -> HOLDING -> CLOSING -> IDLE`).
   - Áp dụng chính sách an toàn `PAUSE_AND_HOLD` (dừng băng tải khi mất kết nối hoặc quá hạn nhận diện) hoặc `DIVERT_TO_DEFAULT` (cho trôi về Bin 3).
3. **`task_mqtt` (Priority 4):**
   - Quản lý kết nối MQTT Client với cơ chế tự động kết nối lại (exponential backoff: 3s, 5s, 10s, 30s).
   - Publish định kỳ Telemetry (1.5s), Heartbeat (2.0s), trạng thái hệ thống, kết quả phân loại, cảnh báo kẹt và cảnh báo nhiệt SoC.
4. **`task_http_tele` (Priority 2):**
   - Gửi HTTP POST định kỳ 5s lên backend Node.js (`http://<DASHBOARD_IP>:5000/api/telemetry`).
   - Tuyệt đối không gửi trường `optical_sensor` để tránh xung đột với logic tự động dừng của backend.
5. **`task_safety` (Priority 5):**
   - Đọc cảm biến nhiệt độ nội chip SoC ESP32-C5 qua `driver/temperature_sensor.h` (chu kỳ 2s). Cảnh báo lên MQTT khi nhiệt độ chip vượt 75°C.
   - Tự động bật SoftAP + Captive Portal nếu mất kết nối Wi-Fi kéo dài.

---

## 5. Hướng Dẫn Cấu Hình, Build & Nạp Code

### 5.1 Kích hoạt môi trường ESP-IDF (PowerShell)
```powershell
. "C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1"
```

### 5.2 Thiết lập Target ESP32-C5
```powershell
cd d:\PBL3_NHOM5\sortix-firmware
idf.py set-target esp32c5
```

### 5.3 Tùy biến thông số cấu hình (Menuconfig)
```powershell
idf.py menuconfig
```
Tại menu **SortiX Firmware Configuration**, có thể điều chỉnh:
- Wi-Fi SSID / Password mặc định.
- Địa chỉ MQTT Broker URI (mặc định: `mqtt://192.168.1.100:1883`).
- Endpoint HTTP Telemetry Backend (mặc định: `http://192.168.1.100:5000/api/telemetry`).
- Chính sách an toàn khi mất Vision (`PAUSE_AND_HOLD` hoặc `DIVERT_TO_DEFAULT`).

### 5.4 Biên dịch (Build)
```powershell
idf.py build
```

### 5.5 Nạp Firmware & Mở Console Monitor
```powershell
idf.py -p COM_PORT flash monitor
```
*(Thay `COM_PORT` bằng cổng COM thực tế của bo WeAct, ví dụ `COM3` hoặc `COM5`).*

---

## 6. Quy Tắc An Toàn Nguồn & E-Stop

1. **E-Stop Cơ Khí:** Nút E-Stop ngắt trực tiếp nguồn 12V trước tiếp điểm K1 và module XL4015 nuôi ESP32. Khi nhấn E-Stop, ESP32 mất nguồn hoàn toàn.
2. **Khởi động An Toàn:** Khi có điện trở lại, hệ thống **LUÔN LUÔN** khởi động vào trạng thái `IDLE`. Băng tải không bao giờ tự ý quay khi chưa có lệnh START từ nút vật lý (GPIO23) hoặc Dashboard.
3. **Lệnh ESTOP_RELEASE:** Lệnh nhả E-Stop từ Dashboard chỉ có tác dụng mở khóa an toàn phần mềm, đưa hệ thống về `IDLE`, **TUYỆT ĐỐI KHÔNG ĐƯỢC** tự khởi động động cơ.
