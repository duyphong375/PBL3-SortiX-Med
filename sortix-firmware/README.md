# SortiX Firmware (ESP-IDF v5.5.x)
### Vi điều khiển: ESP32-C5 RISC-V 32-bit Single-Core @ 240 MHz (WeAct Studio ESP32-C5-A-N8R8)

---

## 1. Giới Thiệu
Mã nguồn firmware điều khiển hệ thống băng tải phân loại dụng cụ y tế tự động SortiX. Firmware kết nối với trạm AI thị giác máy tính (**YOLO11** + FastAPI MJPEG Server chạy trên PC trong thư mục `SortiX-Dashboard/ai-server`) và ứng dụng điều khiển trung tâm **SortiX-Dashboard** thông qua giao thức **MQTT** (MQTTS TLS) và **HTTP REST**.

---

## 2. Toàn Bộ Cấu Trúc Thư Mục & Vai Trò Từng Thành Phần

```text
sortix-firmware/
├── CMakeLists.txt                          # CMake project gốc, kết nối toolchain ESP-IDF
├── sdkconfig.defaults                      # Cấu hình cứng target esp32c5, Flash 8MB, PSRAM 8MB
├── partitions.csv                          # Bảng phân vùng Flash 8MB (NVS, Otadata, OTA_0, OTA_1, Storage)
├── flash_firmware.bat                      # Script nạp nhanh binary firmware qua esptool (Windows Batch)
├── flash_firmware.ps1                      # Script nạp nhanh binary firmware qua esptool (PowerShell)
├── README.md                               # Sổ tay kiến trúc, pinout, hướng dẫn build firmware
│
├── main/                                   # THƯ MỤC CHA 1: Ứng dụng chính & Định nghĩa lõi
│   ├── CMakeLists.txt                      # Đăng ký main.c và khai báo các component phụ thuộc
│   ├── idf_component.yml                   # Khai báo dependency cJSON (espressif/cjson)
│   ├── main.c                              # Khởi tạo app_main(), 5 FreeRTOS Tasks & đăng ký Callbacks
│   ├── app_config.h                        # Trỏ về components/nvs_config/include/app_config.h (single source of truth)
│   ├── pin_definitions.h                   # Sơ đồ gán chân GPIO chuẩn hóa theo BOM danh-sach-linh-kien.md
│   └── version.h                           # Định nghĩa thông tin phiên bản firmware v1.0.0
│
├── components/                             # THƯ MỤC CHA 2: Các module chức năng độc lập (14 components)
│   ├── motor_control/                      # Điều khiển động cơ DC qua L298N (IN1: GPIO8, IN2: GPIO9)
│   │   ├── include/motor_control.h         # Header: forward, reverse, dynamic brake, coast/stop
│   │   ├── motor_control.c                 # Code thực thi: Mutex bảo vệ trạng thái, ghi mức GPIO / LEDC PWM
│   │   └── CMakeLists.txt
│   │
│   ├── encoder/                            # Đọc encoder 2 pha dùng New PCNT Driver (driver/pulse_cnt.h)
│   │   ├── include/encoder.h               # Header: đọc count 64-bit, reset, quy đổi counts <-> mm (63.0 counts/mm)
│   │   ├── encoder.c                       # Code thực thi: Quadrature x4, bộ lọc nhiễu 1us, watchpoint overflow
│   │   └── CMakeLists.txt
│   │
│   ├── servo/                              # Điều khiển 2 servo MG996R 360° non-blocking qua LEDC PWM (GPIO6, GPIO7)
│   │   ├── include/servo.h                 # Header: trigger non-blocking, reset, đọc FSM state
│   │   ├── servo.c                         # Code thực thi: FSM (IDLE->OPENING->HOLDING->CLOSING->IDLE) qua esp_timer
│   │   └── CMakeLists.txt
│   │
│   ├── sensors/                            # Quản lý cảm biến quang và cảm biến ánh sáng
│   │   ├── include/sensors.h               # Header: phát hiện S1 chốt xung encoder và timestamp, đọc LDR
│   │   ├── sensors.c                       # Code thực thi: Lấy mẫu 5ms GPIO3 (15ms blocked / 125ms clear, không cần RC), đọc DO GPIO5
│   │   └── CMakeLists.txt
│   │
│   ├── button/                             # Nút nhấn Start/Stop vật lý (GPIO23)
│   │   ├── include/button.h                # Header: callback khi nhấn nút Start/Stop (toggle + 300ms cooldown)
│   │   ├── button.c                        # Code thực thi: Lấy mẫu 10ms GPIO23 kèm debounce phần mềm 30ms (Pull-up nội)
│   │   └── CMakeLists.txt
│   │
│   ├── led_indicator/                      # Đèn LED trạng thái màu xanh lá (GPIO10)
│   │   ├── include/led_indicator.h         # Header: chế độ OFF, IDLE (1Hz), RUNNING (ON), JAMMED (5Hz)
│   │   ├── led_indicator.c                 # Code thực thi: Quản lý nhấp nháy qua software timer 100ms
│   │   └── CMakeLists.txt
│   │
│   ├── wifi_manager/                       # Quản lý kết nối Wi-Fi Station (STA)
│   │   ├── include/wifi_manager.h          # Header: kiểm tra trạng thái, đọc RSSI, lấy chuỗi IP và Band
│   │   ├── wifi_manager.c                  # Code thực thi: Tự động kết nối lại Hotspot/AP, nhận diện 2.4GHz / 5GHz Wi-Fi 6
│   │   └── CMakeLists.txt
│   │
│   ├── mqtt_handler/                       # MQTT Client đồng bộ 100% schemas & topics SortiX-Dashboard
│   │   ├── include/mqtt_handler.h          # Header: publish telemetry, heartbeat, classification, jam, temp
│   │   ├── mqtt_handler.c                  # Code thực thi: Subscribe vision, control, config/set, estop, TLS broker
│   │   └── CMakeLists.txt
│   │
│   ├── http_telemetry/                     # Gửi HTTP POST telemetry lên backend Node.js port 5000
│   │   ├── include/http_telemetry.h        # Header: gửi payload định kỳ 5s
│   │   ├── http_telemetry.c                # Code thực thi: Đóng gói JSON kèm header X-Device-Token (không gửi optical_sensor)
│   │   └── CMakeLists.txt
│   │
│   ├── sorting_engine/                     # Trọng tâm điều khiển logic phân loại
│   │   ├── include/sorting_engine.h        # Header: queue 15 phôi, matching FIFO, apply config động
│   │   ├── sorting_engine.c                # Code thực thi: Tạm dừng 2s tại cam (100mm), kích hoạt Servo 1 (250mm) / Servo 2 (400mm) / End (650mm)
│   │   └── CMakeLists.txt
│   │
│   ├── state_machine/                      # Quản lý trạng thái hệ thống
│   │   ├── include/state_machine.h         # Header: SYSTEM_STATE_IDLE, RUNNING, STOPPED, JAMMED, ESTOP
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
│   │   ├── include/app_config.h            # Nguồn chuẩn duy nhất (Single Source of Truth) về topics & hằng số
│   │   ├── nvs_config.c                    # Code thực thi: Ghi đọc NVS namespace sortix_cfg & đồng bộ khoảng cách thực tế
│   │   └── CMakeLists.txt
│   │
│   └── captive_portal/                     # Web server nội bộ (port 80) hỗ trợ cấu hình Wi-Fi/MQTT/Token
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

| Chức năng                   | GPIO       | Vị trí Header WeAct | Chế độ / Cấu hình                 |
|-----------------------------|------------|---------------------|-----------------------------------|
| Cảm biến vật cản IR (S1)    | **GPIO3**  | Trái Pin 4          | Input Pull-up, Polling 5ms (15ms blocked / 125ms clear) |
| Encoder Kênh A (Xung)       | **GPIO1**  | Trái Pin 6          | PCNT Pulse Counter (Edge Input)   |
| Encoder Kênh B (Chiều)      | **GPIO4**  | Phải Pin 8          | PCNT Pulse Counter (Level Input)  |
| Cảm biến quang trở LDR (DO) | **GPIO5**  | Phải Pin 9          | Input Digital Pull-up             |
| Servo 1 - PWM (Gạt 1)       | **GPIO6**  | Trái Pin 7          | LEDC PWM 50Hz (Timer 0, Ch 0)     |
| Driver L298N IN1            | **GPIO8**  | Trái Pin 9          | Output / LEDC PWM                 |
| Driver L298N IN2            | **GPIO9**  | Trái Pin 10         | Output / LEDC PWM                 |
| LED trạng thái (Xanh lá)    | **GPIO10** | Trái Pin 11         | Output, 100ms Software Timer      |
| Nút Start/Stop vật lý       | **GPIO23** | Phải Pin 5          | Input Pull-up, Polling 10ms (Debounce 30ms) |
| Servo 2 - PWM (Gạt 2)       | **GPIO7**  | Trái Pin 8          | LEDC PWM 50Hz (Timer 0, Ch 1)     |

### ⚠ CÁC CHÂN CẤM SỬ DỤNG (FORBIDDEN GPIOs):
- **GPIO15 (SPICS1):** Đã nối cứng nội bộ với IC PSRAM 8MB Quad SPI bên trong nắp shield kim loại.
- **GPIO13 / GPIO14:** Cổng USB Native (D- / D+) dùng nạp code, JTAG và USB-CDC Debug.
- **GPIO11 / GPIO12:** Cổng UART0 Console (TX0 / RX0 qua chip CH343).
- **GPIO26 / GPIO27 / GPIO28:** Chân cấu hình khởi động (Strapping Pins), RGB LED và nút BOOT onboard.

---

## 4. Bảng Tra Cứu MQTT Topics (Đồng Bộ 100% Với SortiX-Dashboard)

| Nhóm | Topic MQTT | Hướng truyền | Mục đích / Payload |
|---|---|:---:|---|
| **Vision AI** | `sorter/sorter_01/vision` | Trạm AI -> Firmware | Nhận diện kết quả phân loại: `{ item_id, brand, confidence, timestamp }` |
| **Điều khiển** | `sorter/sorter_01/control` | Dashboard -> Firmware | Lệnh vận hành: `{ command: "START" \| "STOP" \| "RESET", auth_token }` |
| **Cấu hình** | `sorter/sorter_01/config/set` | Dashboard -> Firmware | Cập nhật cấu hình phân loại: `{ version, bins, apply_mode, auth_token }` |
| **Trạng thái cấu hình** | `sorter/sorter_01/config/status` | Firmware -> Dashboard | Phản hồi xác nhận cấu hình: `{ version, status: "APPLIED", timestamp }` |
| **Telemetry** | `sorter/sorter_01/telemetry` | Firmware -> Dashboard | Trạng thái định kỳ 1.5s: `{ state, speed, encoder_count, current_temp }` |
| **Trạng thái máy** | `sorter/sorter_01/status` | Firmware -> Dashboard | Trạng thái máy tức thời: `{ state: "IDLE" \| "RUNNING" \| "JAMMED", uptime }` |
| **Phân loại** | `sorter/sorter_01/classification` | Firmware -> Dashboard | Báo cáo phân loại hoàn tất: `{ item_id, routed_bin, brand, status }` |
| **Heartbeat** | `conveyor/heartbeat` | Firmware -> Dashboard | Nhịp tim định kỳ 2.0s để Watchdog giám sát sống còn |
| **Kẹt phôi** | `conveyor/sensor/jam` | Firmware -> Dashboard | Cảnh báo kẹt động cơ / cơ cấu băng tải (>1.8s không xung) |
| **Nhiệt SoC** | `conveyor/telemetry/temp` | Firmware -> Dashboard | Cảnh báo nhiệt độ chip vượt ngưỡng an toàn (>75°C) |
| **Dừng khẩn cấp** | `conveyor/safety/estop` | Dashboard -> Firmware | Lệnh dừng khẩn cấp: `{ event: "estop", auth_token, reason }` |
| **ACK Dừng khẩn** | `conveyor/safety/estop/ack` | Firmware -> Dashboard | Xác nhận dừng an toàn: `{ event: "estop_ack", state: "SYSTEM_STATE_ESTOP" }` |
| **Giải phóng E-Stop** | `conveyor/safety/estop/release` | Dashboard -> Firmware | Lệnh giải phóng E-Stop: `{ event: "estop_release", auth_token }` |

---

## 5. Kiến Trúc Nhiệm Vụ FreeRTOS & An Toàn

1. **`task_conveyor` (Priority 6):**
   - Thực thi chu kỳ 20ms giám sát an toàn và phát hiện kẹt cơ khí (Encoder Stall): khi động cơ đang chạy, nếu xung encoder không thay đổi quá 1.8s (ngoài thời gian ân hạn khởi động 1.0s), hệ thống kích hoạt phanh động cơ qua `motor_control` và chuyển sang trạng thái `JAMMED`.
2. **`task_sorting` (Priority 5):**
   - Quản lý bộ đệm xoay vòng FIFO (Tracking Queue) tối đa 15 vật thể trên băng tải, cập nhật mỗi 15ms.
   - Nhận sự kiện từ S1 (GPIO3) kèm giá trị xung encoder (`CALIB_ENCODER_COUNTS_PER_MM = 63.0f`) và timestamp vi giây; hỗ trợ **Auto-Wakeup** khi đang ở `IDLE` (khi MCU đã awake) và **Auto-Standby** sau 15s không có vật cản.
   - Dừng băng tải 2.0s tại buồng quét camera (`DIST_S1_TO_CAMERA_MM = 100.0f`) để DroidCam/YOLO11 nhận diện và ghép cặp kết quả Vision AI từ MQTT (`sorter/sorter_01/vision`) theo thứ tự FIFO.
   - Điều khiển Servo 1 (GPIO6 tại `250.0f mm`) và Servo 2 (GPIO7 tại `400.0f mm`) qua FSM Non-blocking (`IDLE -> OPENING -> HOLDING -> CLOSING -> IDLE`, servo 360° 500µs/2500µs/1500µs) hoặc đưa thẳng về Khay 3 tại `650.0f mm` (dung sai kích hoạt `±15.0 mm`).
   - Áp dụng `PAUSE_AND_HOLD` hoặc `DIVERT_TO_DEFAULT` (mặc định hiện tại: `FALLBACK_POLICY_DIVERT_TO_DEFAULT`); bản ghi có `routed_bin` để thống kê hướng firmware đã điều khiển, còn `actual_bin: 0` và trạng thái `unverified` nếu chưa có cảm biến xác nhận phôi vào khay.
3. **`task_mqtt` (Priority 4):**
   - Quản lý kết nối MQTT Client (MQTTS TLS) với cơ chế tự động kết nối lại.
   - Publish định kỳ Telemetry (1.5s), Heartbeat (2.0s), trạng thái hệ thống, kết quả phân loại, cảnh báo kẹt và cảnh báo nhiệt SoC.
4. **`task_http_tele` (Priority 2):**
   - Gửi HTTP/HTTPS POST định kỳ 5s lên backend Node.js (`DEFAULT_HTTP_TELEMETRY_URL` hoặc URL cấu hình trong NVS) kèm header `X-Device-Token` (fallback sang `control_secret` nếu chưa đặt token riêng).
   - Tuyệt đối không gửi trường `optical_sensor` để tránh xung đột với logic tự động dừng của backend.
5. **`task_safety` (Priority 5):**
   - Đọc cảm biến nhiệt độ nội chip SoC ESP32-C5 qua `driver/temperature_sensor.h` (chu kỳ giám sát trong `safety_monitor`). Cảnh báo lên MQTT khi nhiệt độ chip vượt 75°C.
   - Theo dõi trạng thái Wi-Fi Station liên tục để log và tự động kết nối lại Hotspot khi có mạng.

---

## 6. Hướng Dẫn Cấu Hình, Build & Nạp Code

### 6.1 Kích hoạt môi trường ESP-IDF (PowerShell)
```powershell
. "C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1"
```

### 6.2 Thiết lập Target ESP32-C5
```powershell
cd d:\PBL3_NHOM5\sortix-firmware
idf.py set-target esp32c5
```

### 6.3 Cấu hình mặc định & NVS (`app_config.h` / `nvs_config.c`)
- Nguồn cấu hình chuẩn duy nhất nằm tại `components/nvs_config/include/app_config.h` (được `main/app_config.h` include trực tiếp).
- Firmware hiện cấu hình Wi-Fi ở chế độ **Station (STA) Hotspot** (`wifi_manager.c`; SoftAP đang tắt trong `wifi_manager_start_softap()`), sử dụng thông số mặc định từ `app_config.h` (`DEFAULT_WIFI_SSID`, `DEFAULT_WIFI_PASS`, `DEFAULT_MQTT_BROKER_URI = "mqtts://broker.emqx.io:8883"`, `DEFAULT_HTTP_TELEMETRY_URL`, `SORTIX_CONTROL_SECRET`) hoặc giá trị đã lưu trong NVS (`sortix_cfg`).
- `nvs_config_init()` luôn đồng bộ lại các hằng số cơ khí thực tế (`CALIB_ENCODER_COUNTS_PER_MM = 63.0f`, `DIST_S1_TO_SERVO1_MM = 250.0f`, `DIST_S1_TO_SERVO2_MM = 400.0f`, `DIST_S1_TO_END_MM = 650.0f`, `SORTIX_FALLBACK_POLICY`) từ `app_config.h` để tránh lệch thông số cũ trong Flash.
- Component `captive_portal` (port 80) vẫn sẵn sàng trong codebase để nhận form POST `/save` (Wi-Fi, MQTTS/WSS URI, `Control Secret`, `Device API Token`, `HTTPS Telemetry URL`) nếu cần mở portal cấu hình.

### 6.4 Biên dịch (Build) & Nạp Firmware
```powershell
idf.py build
idf.py -p COM12 flash monitor
```
*(Hoặc chạy nhanh bằng `flash_firmware.bat` / `flash_firmware.ps1`, hoặc các script `05_Build_Firmware_ESP32C5.bat` và `06_Nap_Code_Va_Monitor_ESP32C5.bat` trong thư mục `Khoidong/`).*

---

## 7. Giao Thức E-Stop & Mở Khóa An Toàn

1. **E-Stop Vật Lý (Hard-wired Emergency Stop):**
   - Thiết kế mạch E-Stop phần cứng ngắt nguồn động cơ 12V trước tiếp điểm contactor/rơ-le, độc lập với MCU, mạng và phần mềm.
   - Khi E-Stop ngắt nguồn, toàn bộ cơ cấu chấp hành mất điện tức thời.
   - Khi cấp điện trở lại, hệ thống luôn khởi động về trạng thái `SYSTEM_STATE_IDLE` an toàn, chờ lệnh `START` có chủ đích của con người.

2. **Giao Thức E-Stop Phần Mềm & ACK Phần Cứng:**
   - Firmware chỉ chấp nhận E-Stop MQTT có `auth_token` hợp lệ; lệnh từ topic điều khiển dùng cùng cơ chế xác thực.
   - Khi nhận E-Stop, firmware chốt `SYSTEM_STATE_ESTOP`, yêu cầu phanh động cơ và đưa duty điều khiển servo về 0. 
   - Hàm `mqtt_handler_publish_estop_ack()` sẽ phát ACK lên topic `conveyor/safety/estop/ack` để báo trạng thái đã dừng an toàn cho Dashboard.

3. **Luồng Mở Khóa An Toàn (ESTOP_RELEASE):**
   - Chỉ tài khoản Quản trị viên (Admin) được phép kích hoạt mở khóa từ Dashboard, bắt buộc phải nhập ghi chú hiện trường.
   - Dashboard gửi lệnh `ESTOP_RELEASE` kèm trường xác thực `auth_token` tới firmware.
   - Firmware xác minh `auth_token` khớp với `control_secret` lưu trong NVS trước khi xử lý.
   - Khi mở khóa thành công, firmware chỉ chuyển về `SYSTEM_STATE_IDLE` nếu GPIO dừng động cơ được đặt thành công; hệ thống **không tự khởi động lại động cơ**.
   - Nếu còn vật phẩm chưa được xử lý trên băng tải (`sorting_engine_has_unresolved_items()`), hệ thống chặn lệnh START cho đến khi người vận hành xác nhận giải phóng vật thể an toàn.
