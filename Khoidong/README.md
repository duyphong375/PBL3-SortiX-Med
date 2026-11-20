# Thư Mục Khởi Động Hệ Thống (SortiX-Med Scripts)

Thư mục này chứa các kịch bản thực thi Batch script (`.bat`) được chuẩn hóa để khởi động tự động toàn bộ hoặc từng phân hệ trong dự án SortiX-Med.

---

## 1. Danh Sách Các File Khởi Động

| File Script | Chức Năng Chính | Chi Tiết Thực Thi |
|---|---|---|
| **`MENU_KHOI_DONG.bat`** | **Bảng điều khiển Menu tổng hợp** | Cung cấp giao diện chọn số [0-10] để khởi động từng thành phần, chạy trọn bộ Dashboard + AI Camera (`[1]`), chạy đồng thời Dashboard + AI Camera + Nạp/Monitor ESP32-C5 (`[9]`), hoặc chạy riêng AI Camera (`[10]`). |
| **`01_Khoi_Dong_Tat_Ca_Dashboard.bat`** | **Khởi động trọn bộ Dashboard & AI Camera** | Tự động giải phóng cổng 3000, 5000 & 8000; khởi chạy AI Camera Server (:8000), Frontend Next.js (:3000), Backend Node.js (:5000) và Telegram Bot, sau đó tự bật trình duyệt web. |
| **`02_Chi_Chay_Frontend_3000.bat`** | Chỉ chạy giao diện Web Next.js | Giải phóng cổng 3000 và khởi chạy máy chủ Next.js 14 App Router. |
| **`03_Chi_Chay_Backend_5000.bat`** | Chỉ chạy máy chủ Backend | Giải phóng cổng 5000 và khởi chạy HTTP REST server (`server.ts`). |
| **`04_Chi_Chay_Telegram_Bot.bat`** | Chỉ chạy Telegram Poller | Chạy script lắng nghe và phản hồi lệnh 2 chiều cho Bot `@SortiX_Med_Bot`. |
| **`05_Build_Firmware_ESP32C5.bat`** | Biên dịch mã nguồn Firmware | Kích hoạt profile ESP-IDF v5.5.4, kiểm tra target `esp32c5` và thực thi `idf.py build`. |
| **`06_Nap_Code_Va_Monitor_ESP32C5.bat`** | Nạp Firmware & Xem Log | Tự động quét các cổng COM đang kết nối, cho phép chọn cổng (mặc định COM12) và nạp code (`idf.py flash monitor`). |
| **`07_Mo_Serial_Monitor_ESP32C5.bat`** | Mở Serial Monitor | Xem log debug thời gian thực của ESP32-C5 mà không cần nạp lại mã nguồn (mặc định COM12). |
| **`08_Giai_Phong_Port_3000_5000.bat`** | Giải phóng cổng mạng kẹt | Quét và tắt các tiến trình đang chiếm giữ cổng 3000, 5000 và 8000 (AI Camera). |
| **`09_Chi_Chay_AI_Camera_8000.bat`** | Chỉ chạy AI Camera Server | Giải phóng cổng 8000, cho phép chọn nguồn Camera (Enter = mặc định `http://192.168.137.155:4747/video`, `0`/`1` = webcam/USB, hoặc URL khác) và khởi chạy `SortiX-Dashboard/ai-server/run_camera.bat`. |

---

## 2. Hướng Dẫn Sử Dụng Nhanh

### Cách 1: Sử dụng Menu tương tác (Khuyến nghị)
* Nhấp đúp vào **`MENU_KHOI_DONG.bat`** (hoặc file **`KHOI_DONG_HE_THONG.bat`** ở thư mục gốc).
* Nhập số tương ứng với tác vụ bạn muốn thực hiện và nhấn **Enter** (chọn **`1`** để bật trọn bộ Web + AI Camera).

### Cách 2: Khởi động độc lập từng phần
* Muốn chạy Dashboard + AI Camera: Nhấp đúp **`01_Khoi_Dong_Tat_Ca_Dashboard.bat`**.
* Muốn chạy riêng AI Camera: Nhấp đúp **`09_Chi_Chay_AI_Camera_8000.bat`** (hoặc `SortiX-Dashboard\ai-server\run_camera.bat`).
* Muốn nạp code cho ESP32-C5: Cắm cáp USB, nhấp đúp **`06_Nap_Code_Va_Monitor_ESP32C5.bat`**, nhập cổng COM (hoặc Enter cho `COM12`) rồi nhấn Enter.
