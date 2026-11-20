# SortiX-Med: Hệ Thống Băng Tải Phân Loại Dụng Cụ Y Tế Tự Động
### Edge AI & Biomedical IoT Sorting System (ESP32-C5 RISC-V + Next.js Dashboard + YOLO11)
**GitHub Repository:** [duyphong375/PBL3-SortiX-Med](https://github.com/duyphong375/PBL3-SortiX-Med)

---

## 1. Tổng Quan Toàn Bộ Dự Án

Hệ thống **SortiX-Med** là giải pháp phân loại tự động dụng cụ y tế và chất thải y tế tại nguồn nhằm nâng cao an toàn sinh học, giảm thiểu rủi ro tai nạn nghề nghiệp cho nhân viên y tế (vết thương do vật sắc nhọn) và tối ưu quy trình xử lý tiệt trùng Autoclave trong bệnh viện. 

Hệ thống là sự kết hợp chặt chẽ giữa 3 phân hệ cốt lõi:
1. **Phần cứng nhúng thời gian thực (sortix-firmware):** Vi điều khiển **ESP32-C5** (RISC-V 32-bit @ 240 MHz, Dual-Band Wi-Fi 6, 8 MB Flash + 8 MB PSRAM) điều khiển động cơ DC qua driver L298N, giải mã xung encoder qua New PCNT Hardware Counter (`63.0 counts/mm`), điều khiển 2 cơ cấu servo 360° gạt phôi non-blocking (LEDC PWM) và giám sát an toàn đa tầng (E-Stop, kẹt phôi, quá nhiệt SoC).
2. **Trạm thị giác máy tính biên (`SortiX-Dashboard/ai-server/`):** iPhone DroidCam (Wi-Fi/USB) hoặc Webcam USB kết hợp mô hình AI **YOLO11** (Ultralytics, 3 lớp: `cotton`, `scissor`, `syringe`) nhận diện và phân loại dụng cụ y tế với độ trễ thấp, phát luồng video MJPEG trực tiếp (`http://localhost:8000/video_feed`) lên Dashboard và truyền kết quả phân loại thời gian thực qua MQTT (`sorter/sorter_01/vision`).
3. **Bảng điều khiển trung tâm & Giám sát SCADA (SortiX-Dashboard):** Ứng dụng Next.js 14 App Router kết hợp máy chủ Node.js độc lập (`node:http` cổng `5000`), tích hợp trợ lý **AI Copilot Google Gemini** (`gemini-3.1-flash-lite` và chuỗi model fallback), **Telegram Bot Gateway 2 chiều** (`@SortiX_Med_Bot`), giao diện SCADA Kiosk toàn màn hình Fit-Screen (zero-scroll) và hỗ trợ đa nền tảng (Web, Android APK & iOS PWA).

```
                   ┌────────────────────────────────────────┐
                   │   iPhone DroidCam / Webcam USB (PC)    │
                   └───────────────────┬────────────────────┘
                                       │ (Khung hình vật thể)
                                       ▼
                   ┌────────────────────────────────────────┐
                   │   AI Camera Server (YOLO11 + FastAPI)  │
                   │ (SortiX-Dashboard/ai-server/server.py) │
                   └──────────┬──────────────────┬──────────┘
                              │ MJPEG :8000      │ MQTT: sorter/sorter_01/vision
                              ▼                  ▼
┌───────────────────────────┐      MQTT / HTTP       ┌───────────────────────────┐
│     SortiX-Dashboard      │ ◄────────────────────► │      sortix-firmware      │
│  (Next.js 14 + Node.js)   │  MQTTS 8883 / HTTP 5000│ (ESP32-C5 RISC-V @ 240MHz)│
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

Dự án được tổ chức khoa học theo mô hình đa tầng (multi-tier system) với các thư mục chuyên biệt:

```text
d:/PBL3_NHOM5/
│
├── sortix-firmware/                        # [FOLDER CHA 1] MÃ NGUỒN NHÚNG ESP-IDF v5.5.x
│   ├── CMakeLists.txt                      # Cấu hình build cấp cao nhất cho firmware
│   ├── sdkconfig.defaults                  # Cấu hình target esp32c5, PSRAM 8MB, Flash 8MB
│   ├── partitions.csv                      # Bảng phân vùng bộ nhớ Flash (NVS, Otadata, OTA_0, OTA_1, Storage)
│   ├── flash_firmware.bat                  # Script Windows nạp nhanh firmware qua esptool
│   ├── flash_firmware.ps1                  # Script PowerShell nạp nhanh firmware qua esptool
│   ├── README.md                           # Sổ tay kiến trúc, pinout, hướng dẫn build firmware
│   │
│   ├── main/                               # Entrypoint và cấu hình cốt lõi
│   │   ├── CMakeLists.txt                  # Khai báo source file và liên kết module
│   │   ├── idf_component.yml               # Quản lý thư viện bên ngoài (espressif/cjson)
│   │   ├── main.c                          # Khởi tạo app_main, 5 FreeRTOS Tasks và các callback
│   │   ├── app_config.h                    # Trỏ tới single source of truth trong components/nvs_config
│   │   ├── pin_definitions.h               # Sơ đồ gán chân GPIO chuẩn hóa cho bo WeAct ESP32-C5
│   │   └── version.h                       # Định nghĩa phiên bản firmware v1.0.0
│   │
│   ├── components/                         # 14 module nhúng độc lập (Loose Coupling)
│   │   ├── motor_control/                  # Điều khiển động cơ DC qua L298N (IN1: GPIO8, IN2: GPIO9)
│   │   ├── encoder/                        # Bộ đếm xung New PCNT quadrature x4 (Kênh A: GPIO1, Kênh B: GPIO4)
│   │   ├── servo/                          # Điều khiển 2 Servo MG996R 360° non-blocking qua LEDC PWM (GPIO6, GPIO7)
│   │   ├── sensors/                        # Lấy mẫu phần mềm IR S1 (GPIO3) và đọc quang trở LDR (GPIO5)
│   │   ├── button/                         # Lấy mẫu phần mềm nút nhấn Start/Stop vật lý (GPIO23, debounce 30ms)
│   │   ├── led_indicator/                  # Quản lý hiệu ứng LED xanh trạng thái (GPIO10) qua timer 100ms
│   │   ├── wifi_manager/                   # Kết nối Wi-Fi Station (2.4GHz & 5GHz Wi-Fi 6), tự động reconnect Hotspot
│   │   ├── mqtt_handler/                   # Client MQTT TLS (mqtts://) đồng bộ 100% schemas & topics với SortiX-Dashboard
│   │   ├── http_telemetry/                 # Gửi HTTP POST telemetry định kỳ 5s lên backend port 5000
│   │   ├── sorting_engine/                 # Tracking queue 15 phôi, ghép cặp Vision FIFO, gán khay động
│   │   ├── state_machine/                  # Máy trạng thái hệ thống: IDLE, RUNNING, STOPPED, JAMMED, ESTOP
│   │   ├── safety_monitor/                 # Phát hiện kẹt cơ khí (Encoder Stall) & đo nhiệt SoC nội chip
│   │   ├── nvs_config/                     # Quản lý đọc/ghi cấu hình bền vững NVS & chứa app_config.h chuẩn
│   │   └── captive_portal/                 # Web server nội bộ (port 80) cấu hình Wi-Fi/MQTT/Token khi cần
│   │
│   └── test/                               # Thư mục kiểm thử phần mềm tự động (Unity Framework)
│       ├── CMakeLists.txt                  # Cấu hình biên dịch runner test
│       ├── test_encoder.c                  # Test đơn vị chuyển đổi xung sang mm và chống tràn
│       ├── test_sorting_engine.c           # Test đơn vị ring buffer FIFO, vision matching, dynamic config
│       └── test_state_machine.c            # Test đơn vị an toàn khởi động IDLE, E-Stop release
│
├── SortiX-Dashboard/                       # [FOLDER CHA 2] GIAO DIỆN WEB, BACKEND & AI CAMERA SERVER
│   ├── ai-server/                          # Trạm Thị Giác Máy Tính Biên (YOLO11 + FastAPI MJPEG :8000 + MQTT)
│   │   ├── server.py                       # Server chính: đọc DroidCam/Webcam, nhận diện YOLO11, phát /video_feed & /health
│   │   ├── capture_samples.py              # Công cụ chụp ảnh mẫu nhanh trên băng tải theo lớp
│   │   ├── auto_label.py                   # Công cụ tự động gán nhãn tách nền băng tải xanh + YOLO fallback
│   │   ├── train.py                        # Huấn luyện mô hình YOLO11 3 lớp (cotton, scissor, syringe)
│   │   ├── detect_iphone.py                # Script inference thử nghiệm trực tiếp từ iPhone/Webcam
│   │   ├── data.yaml                       # Cấu hình dataset YOLO11 3 lớp (cotton, scissor, syringe)
│   │   ├── requirements.txt                # Thư viện Python (ultralytics, fastapi, uvicorn, opencv-python, paho-mqtt)
│   │   ├── run_camera.bat                  # Script khởi chạy nhanh AI Camera Server (cổng 8000)
│   │   ├── weights/best.pt                 # Trọng số YOLO11 đã huấn luyện sẵn sàng deploy
│   │   ├── dataset/                        # Bộ dữ liệu huấn luyện 3 lớp (train: 954, valid: 150, test: 101 ảnh)
│   │   └── runs/sortix_med_3class/         # Biểu đồ, kết quả huấn luyện (80 epochs) và checkpoints
│   ├── backend/                            # Native HTTP server (Node.js, TypeScript) port 5000
│   │   ├── database/                       # Migrations và Seeds tham chiếu (PostgreSQL, MySQL, SQLite, MongoDB)
│   │   └── src/                            # server.ts, controllers, routes, middlewares, services, models
│   ├── frontend/                           # Giao diện Next.js 14 App Router, Tailwind CSS, Lucide icons
│   │   ├── src/app/                        # Dashboard thời gian thực, Băng tải, Thống kê, Lịch sử, Cấu hình, Users, API
│   │   ├── src/components/                 # layout, overview, conveyor, ui, users, widgets, AiCopilot
│   │   └── capacitor.config.ts             # Cấu hình Capacitor 8 đóng gói Android APK & iOS PWA
│   ├── shared/                             # Thư viện dùng chung giữa Frontend, Backend và Firmware
│   │   ├── schemas/                        # Zod schemas (TelemetrySchema, VisionDetectionSchema, SorterConfigSchema, v.v.)
│   │   ├── constants/                      # Hằng số topics MQTT, định danh 3 khay phân loại mặc định
│   │   └── types/                          # TypeScript interfaces dùng chung (CATALOG_BRANDS 3 lớp + alias tương thích)
│   ├── data/                               # Bộ lưu trữ JSON runtime bền vững (history, notifications, sync_state, telemetry_logs, users)
│   ├── scripts/                            # dev-all.cjs, telegram-poller.cjs, e2e.cjs, free-port.cjs, dev.ps1, test.ps1
│   ├── tests/                              # Bộ kiểm thử 26 test suites tự động (182/182 unit tests PASS)
│   ├── docs/                               # architecture.md, api.md
│   ├── package.json                        # Monorepo workspaces cấu hình dependencies & scripts
│   └── vercel.json                         # Cấu hình triển khai tự động lên Vercel
│
├── Khoidong/                               # [FOLDER CHA 3] BỘ SCRIPT ĐIỀU PHỐI KHỞI ĐỘNG NHANH WINDOWS
│   ├── MENU_KHOI_DONG.bat                  # Menu tổng hợp [0-10] điều phối Dashboard, AI Camera & Firmware ESP32-C5
│   ├── 01_Khoi_Dong_Tat_Ca_Dashboard.bat   # Chạy đồng thời AI Camera (:8000), Frontend (:3000), Backend (:5000) & Telegram Bot
│   ├── 02..09_*.bat                        # Các script chạy độc lập từng phân hệ, build/flash/monitor ESP32-C5 & giải phóng cổng
│   └── README.md                           # Tài liệu hướng dẫn sử dụng bộ script Khoidong
│
├── KHOI_DONG_HE_THONG.bat                  # Phím tắt gốc mở nhanh Khoidong/MENU_KHOI_DONG.bat
├── danh-sach-linh-kien.md                  # [TÀI LIỆU PHẦN CỨNG 1] BOM linh kiện, đặc tính điện, bảo vệ nguồn, sơ đồ đấu nối
├── ESP32-C5-WROOM-1-thong-tin-pinout.md    # [TÀI LIỆU PHẦN CỨNG 2] Thuyết minh chi tiết các chân rào J6/J7 bo WeAct
├── huong-dan-backup-git.md                 # [TÀI LIỆU QUY TRÌNH] Cẩm nang quản lý Git, chiến lược backup & workflow nhóm
├── ESP32-C5-Pinout.png                     # [HÌNH ẢNH PHẦN CỨNG] Sơ đồ trực quan pinout chân vi điều khiển
├── ESP32-C5-WROOM-1.PDF                    # [DATASHEET CHÍNH THỨC] Tài liệu kỹ thuật SoC ESP32-C5 từ Espressif
└── vercel.json                             # Cấu hình root triển khai monorepo lên Vercel
```

---

## 3. Vai Trò Chi Tiết Của Từng Folder Cha

### 3.1. Thư mục `sortix-firmware/`
- **Mục đích:** Chịu trách nhiệm vận hành thời gian thực nghiêm ngặt (Hard Real-Time) của toàn bộ cơ cấu chấp hành cơ khí.
- **Nền tảng:** ESP-IDF v5.5.x, chuẩn C11, target `esp32c5`.
- **Cơ chế hoạt động:**
  - `task_conveyor` (Priority 6): Điều khiển cầu H L298N và giám sát an toàn kẹt cơ khí (Encoder Stall) khi motor quay nhưng trục không chuyển động quá 1.8s (sau 1.0s grace startup).
  - `task_sorting` (Priority 5): Quản lý Tracking Ring Buffer (tối đa 15 phôi trên băng tải). Khi vật cản đi qua cảm biến IR S1 (GPIO3), cảm biến lấy mẫu chốt xung encoder (`63.0 counts/mm`) và timestamp; băng tải tạm dừng 2.0s tại buồng quét camera (`100 mm`) để nhận diện, ghép cặp kết quả Vision AI từ MQTT theo FIFO và kích hoạt Servo 1 (`250 mm`, GPIO6) hoặc Servo 2 (`400 mm`, GPIO7) theo FSM non-blocking (hoặc đi thẳng vào Khay 3 tại `650 mm`), kèm cơ chế Auto-Standby sau 15s không có vật.
  - `task_mqtt` (Priority 4): Gửi định kỳ Telemetry (1.5s), Heartbeat (2.0s), kết quả phân loại (`classification`), cảnh báo kẹt (`conveyor/sensor/jam`) và nhiệt SoC (`conveyor/telemetry/temp`) qua MQTTS TLS.
  - `task_http_tele` (Priority 2): Định kỳ 5s gửi dữ liệu HTTP POST lên Backend Node.js port 5000 (`/api/telemetry`).
  - `task_safety` (Priority 5): Giám sát trạng thái kết nối Wi-Fi Station liên tục (tự động reconnect Hotspot) và phối hợp giám sát nhiệt độ lõi chip qua `safety_monitor`.

### 3.2. Thư mục `SortiX-Dashboard/`
- **Mục đích:** Bảng điều khiển trung tâm dành cho người vận hành y tế, điều dưỡng và kỹ sư bảo trì, đồng thời tích hợp Trạm AI Camera (`ai-server/`).
- **Tính năng nổi bật:**
  - **Giám sát thời gian thực 60fps & Luồng Camera AI trực tiếp**: Trực quan hóa băng chuyền phân loại 3 nhóm dụng cụ y tế chính (`med_syringe` - Kim tiêm, `med_scissors` - Kéo, `med_cotton` - Bông; hỗ trợ alias tương thích ngược) theo 3 khay chứa chuyên dụng, kèm khung video MJPEG trực tiếp từ AI Camera Server (`:8000/video_feed`).
  - **Trợ lý AI Copilot 2 Chiều & Telegram Bot Gateway**: Tích hợp mô hình Google Gemini (`gemini-3.1-flash-lite` mặc định kèm danh sách model fallback `gemini-3-flash-preview`, `gemini-3.5-flash-lite`, `gemini-3.8-flash`...) trực tiếp qua API v1beta. Hỗ trợ hỏi đáp thông minh và điều khiển SCADA từ xa qua Telegram Bot (`@SortiX_Med_Bot`) với bàn phím Inline Keyboard an toàn (`/status`, `/temp`, `/estop`, `/start`, `/stop`, `/clear_bin`).
  - **Giao diện SCADA Kiosk & Fit Màn Hình**: Hỗ trợ chuyển đổi mật độ hiển thị (Display Density: Compact / Comfortable) và chế độ toàn màn hình F11 Kiosk không cuộn trang (zero-scroll).
  - **Báo cáo 1 ngày làm việc (Shift Summary)**: Tự động tổng kết sản lượng phân loại, tỷ lệ đạt/lỗi, xuất báo cáo UTF-8 BOM CSV và PDF đạt chuẩn y tế.
  - **Hệ thống an toàn công nghiệp**: Dừng khẩn cấp E-Stop, cảnh báo kẹt phôi, đầy khay, quá nhiệt và tự động phát hiện mất kết nối MQTT.

### 3.3. Thư mục `SortiX-Dashboard/ai-server/` (Trạm Thị Giác Máy Tính YOLO11)
- **Mục đích:** Trạm thị giác máy tính biên (Edge AI Computer Vision) thực hiện nhận diện, phát luồng MJPEG và phân loại thời gian thực dụng cụ y tế đi qua buồng quét camera.
- **Trạng thái dữ liệu & mô hình hiện tại:** Đã hoàn thiện bộ dữ liệu 3 lớp chính thức (**`cotton`**, **`scissor`**, **`syringe`**) gồm **1.205 ảnh** (`train`: 954, `valid`: 150, `test`: 101 ảnh) và huấn luyện 80 epochs với **Ultralytics YOLO11** (`runs/sortix_med_3class/weights/best.pt`, deploy tại `weights/best.pt`).
- **Nền tảng:** Python 3.10+, PyTorch, Ultralytics YOLO11, OpenCV, FastAPI, Uvicorn, Paho-MQTT.
- **Thành phần chính:**
  - `server.py`: Đọc luồng iPhone DroidCam (Wi-Fi URL hoặc USB index) / Webcam, tự động xoay khung hình (`CAMERA_ROTATE=90`), tự động khoanh vùng ROI băng tải xanh, chống ngược sáng, chạy inference YOLO11 (`conf >= 0.6`), publish kết quả sang MQTT topic `sorter/sorter_01/vision` (ánh xạ `syringe -> med_syringe`, `scissor -> med_scissors`, `cotton -> med_cotton`) và phát MJPEG stream tại `http://localhost:8000/video_feed` (kèm endpoint `GET /health`).
  - `capture_samples.py`: Chụp ảnh mẫu nhanh theo từng lớp (`1`: syringe, `2`: scissor, `3`: cotton, `4`: mixed, `0`: background).
  - `auto_label.py`: Tự động gán nhãn chuẩn YOLO bằng cách lọc vùng băng tải xanh theo HSV kết hợp fallback sang mô hình `weights/best.pt`.
  - `train.py`: Script huấn luyện lại mô hình 3 lớp trên `data.yaml` và tự động cập nhật `weights/best.pt`.

### 3.4. Các thư mục & tài liệu hỗ trợ (`Khoidong/`, tài liệu phần cứng & quy trình)
- `Khoidong/` & `KHOI_DONG_HE_THONG.bat`: Bộ script Batch điều phối toàn bộ hệ thống trên Windows (`MENU_KHOI_DONG.bat` với các lựa chọn 0–10: chạy trọn bộ Dashboard + AI Camera, chạy lẻ từng service, build/flash/monitor ESP32-C5, giải phóng cổng 3000/5000/8000).
- `danh-sach-linh-kien.md`: Chuẩn hóa phần cứng, phân tích tải nguồn (12V nguồn chính, 5V buck XL4015, 3.3V bo WeAct) và bảng đấu nối dây.
- `ESP32-C5-WROOM-1-thong-tin-pinout.md`: Thuyết minh chi tiết các chân rào J6/J7 bo WeAct và bảo vệ các chân cấm (GPIO15 PSRAM, GPIO13/14 USB, GPIO11/12 UART0, GPIO26/27/28 Strapping).
- `huong-dan-backup-git.md`: Cẩm nang quản trị Git, chiến lược backup nhiều lớp và quy trình phối hợp nhóm không xung đột.

---

## 4. Hướng Dẫn Vận Hành Nhanh Hệ Thống

### Cách Nhanh Nhất (Windows Menu Điều Phối)
Nhấp đúp vào **`KHOI_DONG_HE_THONG.bat`** tại thư mục gốc (hoặc `Khoidong/MENU_KHOI_DONG.bat`):
- Chọn **`[1]`** để khởi động toàn bộ Dashboard (Frontend `:3000` + Backend `:5000` + Telegram Bot Poller) cùng AI Camera Server (`:8000`).
- Chọn **`[9]`** để khởi động đồng thời cả Dashboard + AI Camera và cửa sổ nạp/monitor ESP32-C5.

### Hoặc Khởi Động Thủ Công Qua Terminal

#### Bước 1: Khởi động Dashboard & Telegram Bot Poller (PC)
```bash
cd d:/PBL3_NHOM5/SortiX-Dashboard
npm run dev:all
```
> **Lưu ý:** Lệnh `npm run dev:all` khởi động đồng thời cả 3 tiến trình:
> - Frontend Next.js: `http://localhost:3000`
> - Backend Node.js API: `http://localhost:5000`
> - Telegram Bot Poller 2 chiều: Kết nối trực tiếp Bot Telegram `@SortiX_Med_Bot` với hệ thống SCADA.

#### Bước 2: Khởi động AI Camera Server (YOLO11 + FastAPI MJPEG Stream)
Mở cửa sổ Terminal thứ hai (hoặc chạy `SortiX-Dashboard/ai-server/run_camera.bat`):
```powershell
cd d:\PBL3_NHOM5\SortiX-Dashboard\ai-server
.\.venv\Scripts\python.exe -u server.py
```
- Luồng video hiển thị trên Dashboard: `http://localhost:8000/video_feed`
- Kiểm tra trạng thái camera & FPS: `http://localhost:8000/health`

#### Bước 3: Nạp Firmware & Giám Sát ESP32-C5
Mở PowerShell ESP-IDF (hoặc dùng `sortix-firmware/flash_firmware.bat`):
```powershell
. "C:\Espressif\tools\Microsoft.v5.5.4.PowerShell_profile.ps1"
cd d:\PBL3_NHOM5\sortix-firmware
idf.py -p COM12 flash monitor
```

#### Bước 4: Vận hành hệ thống
1. Bật nguồn 12V tổng.
2. Kiểm tra đèn LED trạng thái GPIO10 (trạng thái `IDLE` / `RUNNING` / `JAMMED`).
3. Nhấn nút Start/Stop vật lý (GPIO23) hoặc nhấn **START** trên giao diện Web Dashboard (hoặc gửi lệnh `/start` trên Telegram Bot) để băng tải bắt đầu vận hành.

---

## 5. Kiểm Thử Tự Động & Đảm Bảo Chất Lượng (QA)

Toàn bộ hệ thống được bảo vệ bởi bộ kiểm thử tự động toàn diện:
```bash
cd SortiX-Dashboard
npm test
```
- **Phạm vi kiểm thử:** Bao phủ **27 file test suites** tự động (**182/182 unit tests PASS**):
  - An toàn E-Stop & Khóa liên động phần cứng (`estop_safety`, `security_audit_remediation`).
  - Phát hiện kẹt cơ khí & Audio báo động (`jam_detection`, `jam_simulation_audio`).
  - Quản lý đầy khay & Cảnh báo quá nhiệt (`bin_full`, `temperature_warning`, `temperature_gauge_simulation_vs_real`).
  - Giám sát Watchdog & Mất kết nối (`device_offline`, `mqtt_disconnected`).
  - Báo cáo ca làm việc & Đồng bộ đa thiết bị (`shift_summary`, `daily_report_sync`, `cross_device_sync`).
  - Tự động chạy/dừng băng tải, chống phôi ma & đồng bộ 3D/Start-Pause (`auto_stop_on_all_items_in_trays`, `ghost_item_prevention`, `conveyor_3d_start_pause_sync`).
  - Xác thực bảo mật, RBAC & Quản lý người dùng (`users`, `api_schemas`).
  - Giao diện SCADA Kiosk Fit-Screen & Telegram Copilot 2 chiều (`anti_ai_industrial_scada_ui`, `telegram_copilot_2way_and_screenfit`).
