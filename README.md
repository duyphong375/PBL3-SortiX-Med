# SortiX-Med: Hệ Thống Băng Tải Phân Loại Dụng Cụ Y Tế Tự Động
### Edge AI & Biomedical IoT Sorting System (ESP32-C5 RISC-V + Next.js Dashboard + YOLOv8)
**GitHub Repository:** [duyphong375/PBL3-SortiX-Med](https://github.com/duyphong375/PBL3-SortiX-Med)

---

## 1. Tổng Quan Toàn Bộ Dự Án

Hệ thống **SortiX** là giải pháp phân loại tự động dụng cụ và rác thải y tế tại nguồn nhằm nâng cao an toàn sinh học và tối ưu quy trình xử lý vô trùng tại bệnh viện. Hệ thống kết hợp giữa phần cứng nhúng thời gian thực vi điều khiển **ESP32-C5**, mô hình thị giác máy tính **YOLOv8** nhận diện hình ảnh, và ứng dụng giám sát điều khiển tập trung **SortiX-Dashboard**.

```
                   ┌────────────────────────────────────────┐
                   │    Webcam USB 2K / Camera AI (PC)      │
                   └───────────────────┬────────────────────┘
                                       │ (Hình ảnh vật thể)
                                       ▼
                   ┌────────────────────────────────────────┐
                   │      PC Edge AI Station (YOLOv8)       │
                   └───────────────────┬────────────────────┘
                                       │ MQTT: sorter/sorter_01/vision
                                       ▼
┌───────────────────────────┐      MQTT / HTTP       ┌───────────────────────────┐
│     SortiX-Dashboard      │ ◄────────────────────► │      sortix-firmware      │
│  (Next.js 14 + Node.js)   │  Port 1883 / Port 5000 │ (ESP32-C5 RISC-V @ 240MHz)│
└───────────────────────────┘                        └─────────────┬─────────────┘
                                                                   │ GPIO / PCNT / PWM
                                                                   ▼
                                                     ┌───────────────────────────┐
                                                     │     Băng Tải Cơ Khí       │
                                                     │ (L298N, Encoder, 2x Servos│
                                                     │  IR S1, LDR, E-Stop 12V)  │
                                                     └───────────────────────────┘
```

---

## 2. Toàn Bộ Cấu Trúc Dự Án Của Từng Folder Cha

Dự án được phân chia thành các thư mục cha độc lập, rõ ràng theo đúng chuẩn kiến trúc phân tầng:

```text
d:/PBL3_NHOM5/
│
├── sortix-firmware/                        # [FOLDER CHA 1] MÃ NGUỒN NHÚNG ESP-IDF v5.5.x
│   ├── CMakeLists.txt                      # Cấu hình build cấp cao nhất cho firmware
│   ├── sdkconfig.defaults                  # Cấu hình target esp32c5, PSRAM 8MB, Flash 8MB
│   ├── partitions.csv                      # Bảng phân vùng bộ nhớ Flash (NVS, Otadata, OTA_0, OTA_1, Storage)
│   ├── README.md                           # Sổ tay kiến trúc, pinout, hướng dẫn build firmware
│   │
│   ├── main/                               # Thư mục chính chứa entrypoint và cấu hình cốt lõi
│   │   ├── CMakeLists.txt                  # Khai báo source file và liên kết module
│   │   ├── idf_component.yml               # Quản lý thư viện bên ngoài (espressif/cjson)
│   │   ├── Kconfig.projbuild               # Giao diện menuconfig cấu hình Wi-Fi, MQTT, timeout
│   │   ├── main.c                          # Khởi tạo app_main, 5 FreeRTOS Tasks và các ngắt
│   │   ├── app_config.h                    # Định nghĩa topic MQTT, schema, địa chỉ server, hằng số cơ khí
│   │   ├── pin_definitions.h               # Sơ đồ gán chân GPIO chuẩn hóa cho bo WeAct ESP32-C5
│   │   └── version.h                       # Định nghĩa phiên bản firmware v1.0.0
│   │
│   ├── components/                         # Thư mục 14 module nhúng độc lập (Loose Coupling)
│   │   ├── motor_control/                  # Điều khiển động cơ DC qua L298N (IN1: GPIO8, IN2: GPIO9)
│   │   ├── encoder/                        # Bộ đếm xung New PCNT quadrature x4 (Kênh A: GPIO1, Kênh B: GPIO4)
│   │   ├── servo/                          # Điều khiển 2 Servo MG996R non-blocking qua LEDC PWM (GPIO6, GPIO24)
│   │   ├── sensors/                        # Ngắt IR S1 (GPIO0) và đọc quang trở LDR (GPIO5)
│   │   ├── button/                         # Ngắt nút nhấn Start vật lý (GPIO23) + debounce 20ms
│   │   ├── led_indicator/                  # Quản lý hiệu ứng LED xanh trạng thái (GPIO10) qua timer 100ms
│   │   ├── wifi_manager/                   # Kết nối Wi-Fi Station (2.4GHz & 5GHz Wi-Fi 6), tự động chuyển SoftAP
│   │   ├── mqtt_handler/                   # Client MQTT đồng bộ 100% schemas & topics với SortiX-Dashboard
│   │   ├── http_telemetry/                 # Gửi HTTP POST telemetry định kỳ 5s lên backend port 5000
│   │   ├── sorting_engine/                 # Tracking queue 15 phôi, ghép cặp Vision FIFO, gán khay động
│   │   ├── state_machine/                  # Máy trạng thái hệ thống: IDLE, RUNNING, STOPPED, JAMMED
│   │   ├── safety_monitor/                 # Phát hiện kẹt cơ khí (Encoder Stall) & đo nhiệt SoC nội chip
│   │   ├── nvs_config/                     # Quản lý đọc/ghi cấu hình bền vững vào bộ nhớ Flash NVS
│   │   └── captive_portal/                 # Web server nội bộ (port 80) cấu hình Wi-Fi/MQTT qua SoftAP
│   │
│   └── test/                               # Thư mục kiểm thử phần mềm tự động (Unity Framework)
│       ├── CMakeLists.txt                  # Cấu hình biên dịch runner test
│       ├── test_encoder.c                  # Test đơn vị chuyển đổi xung sang mm và chống tràn
│       ├── test_sorting_engine.c           # Test đơn vị ring buffer FIFO, vision matching, dynamic config
│       └── test_state_machine.c            # Test đơn vị an toàn khởi động IDLE, E-Stop release
│
├── SortiX-Dashboard/                       # [FOLDER CHA 2] GIAO DIỆN WEB & BACKEND SERVER TẬP TRUNG
│   ├── backend/                            # Native HTTP server (Node.js, TypeScript) port 5000
│   │   ├── src/controllers/                # telemetryController.ts, safetyController, v.v.
│   │   └── src/server.ts                   # Endpoint /api/telemetry, /api/history, /api/events
│   ├── frontend/                           # Giao diện Next.js 14 App Router, Tailwind CSS, Lucide icons
│   │   └── src/app/                        # Các trang: Dashboard thời gian thực, Giám sát camera AI, Lịch sử
│   ├── shared/                             # Thư viện dùng chung giữa Frontend, Backend và Firmware
│   │   ├── schemas/                        # Zod schemas (TelemetrySchema, ClassificationRecordSchema, v.v.)
│   │   ├── constants/                      # Hằng số topics MQTT, định danh khay phân loại mặc định
│   │   └── types/                          # TypeScript interfaces
│   └── package.json                        # Cấu hình dependencies (Next.js, Zod, MQTT.js, v.v.)
│
├── danh-sach-linh-kien.md                  # [TÀI LIỆU PHẦN CỨNG 1] BOM, đặc tính điện, contact rating, bảo vệ nguồn
├── ESP32-C5-WROOM-1-thong-tin-pinout.md    # [TÀI LIỆU PHẦN CỨNG 2] Thuyết minh chi tiết các chân rào J6/J7 bo WeAct
├── ESP32-C5-Pinout.png                     # [HÌNH ẢNH PHẦN CỨNG] Sơ đồ trực quan pinout chân vi điều khiển
└── ESP32-C5-WROOM-1.PDF                    # [DATASHEET CHÍNH THỨC] Tài liệu kỹ thuật SoC ESP32-C5 từ Espressif
```

---

## 3. Vai Trò Chi Tiết Của Từng Folder Cha

### 3.1. Thư mục `sortix-firmware/`
- **Mục đích:** Chịu trách nhiệm vận hành thời gian thực (Hard Real-Time) của toàn bộ cơ cấu chấp hành cơ khí.
- **Nền tảng:** ESP-IDF v5.5.4, chuẩn C11, target `esp32c5`.
- **Cơ chế hoạt động:**
  - `task_conveyor` (Priority 6): Đọc xung bộ đếm phần cứng PCNT từ encoder motor, phát hiện kẹt cơ khí (Encoder Stall) khi motor quay nhưng trục không chuyển động trong 1.8s.
  - `task_sorting` (Priority 5): Quản lý Tracking Ring Buffer (tối đa 15 phôi trên băng tải). Khi vật cản đi qua cảm biến IR S1 (GPIO0), ngắt cạnh xuống kích hoạt chốt xung encoder và timestamp. Khi có kết quả Vision AI từ MQTT, phôi được ghép cặp FIFO và tính toán cự ly để kích hoạt Servo 1 (GPIO6) hoặc Servo 2 (GPIO24) theo FSM non-blocking.
  - `task_mqtt` (Priority 4): Gửi định kỳ Telemetry (1.5s), Heartbeat (2.0s), kết quả phân loại (`classification`), cảnh báo kẹt (`conveyor/sensor/jam`) và nhiệt SoC (`conveyor/telemetry/temp`).
  - `task_http_tele` (Priority 2): Định kỳ 5s gửi dữ liệu HTTP POST lên Backend Node.js port 5000 (`/api/telemetry`).
  - `task_safety` (Priority 5): Giám sát nhiệt độ lõi chip qua cảm biến nhiệt nội (`driver/temperature_sensor.h`) và quản lý watchdog.

### 3.2. Thư mục `SortiX-Dashboard/`
- **Mục đích:** Bảng điều khiển trung tâm dành cho người vận hành y tế và kỹ sư bảo trì.
- **Tính năng chính:**
  - Giám sát trạng thái băng tải 3D/2D thời gian thực (vị trí phôi, tốc độ, số lượng từng khay).
  - Điều khiển Start / Stop / Mở khóa an toàn E-Stop từ xa.
  - Cấu hình động luật phân loại (`sorter/sorter_01/config/set`): Gán lại brand nào vào khay nào mà không cần nạp lại code ESP32.
  - Xem biểu đồ lịch sử tốc độ, dòng tải ước tính, nhiệt độ SoC và nhật ký cảnh báo kẹt phôi.

### 3.3. Các tài liệu phần cứng (`danh-sach-linh-kien.md`, `ESP32-C5-Pinout.png`, `ESP32-C5-WROOM-1.PDF`)
- Chuẩn hóa phần cứng, phân tích tải nguồn (12V nguồn chính, 5V buck XL4015, 3.3V bo WeAct).
- Quy chuẩn an toàn E-Stop cơ khí: E-Stop ngắt nguồn 12V trước tiếp điểm K1 $\rightarrow$ ESP32 mất điện hoàn toàn. Khi có điện lại, hệ thống luôn khởi động về trạng thái `IDLE` chờ lệnh Start.
- Bảo vệ các chân cấm (GPIO15 PSRAM, GPIO13/14 USB, GPIO11/12 UART0, GPIO26/27/28 Strapping).

---

## 4. Hướng Dẫn Vận Hành Nhanh Hệ Thống

### Bước 1: Khởi động Dashboard (PC)
```bash
cd d:/PBL3_NHOM5/SortiX-Dashboard
npm run dev
```
Truy cập giao diện: `http://localhost:3000` (Backend API chạy song song tại port 5000).

### Bước 2: Nạp Firmware & Giám Sát ESP32-C5
Mở PowerShell:
```powershell
. "C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1"
cd d:\PBL3_NHOM5\sortix-firmware
idf.py -p COM5 flash monitor
```

### Bước 3: Vận hành hệ thống
1. Bật nguồn 12V tổng.
2. Kiểm tra đèn LED trạng thái GPIO10 chớp chậm 1 Hz (Báo hiệu hệ thống đang ở trạng thái `IDLE` an toàn).
3. Nhấn nút Start vật lý (GPIO23) hoặc nhấn **START** trên giao diện Web Dashboard để băng tải bắt đầu vận hành.
