#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * =============================================================================
 * Cấu hình ứng dụng SortiX & Định nghĩa các giao thức truyền thông
 * =============================================================================
 */

// Định danh thiết bị (Thống nhất trên MQTT, HTTP và SortiX-Dashboard)
#define SORTIX_DEVICE_ID                    "sorter_01"

// Chính sách dự phòng khi mất kết nối MQTT hoặc chưa chạy thị giác máy AI
#define FALLBACK_POLICY_PAUSE_AND_HOLD      0   // An toàn: Dừng băng tải ngay lập tức, nhấp nháy LED cảnh báo
#define FALLBACK_POLICY_DIVERT_TO_DEFAULT   1   // Tiếp tục chạy: Cho vật thể trôi về khay mặc định (Khay 3) liên tục không dừng

#ifndef SORTIX_FALLBACK_POLICY
#define SORTIX_FALLBACK_POLICY              FALLBACK_POLICY_DIVERT_TO_DEFAULT
#endif

// =============================================================================
// Các MQTT Topic (Đồng bộ 100% với SortiX-Dashboard DEFAULT_MQTT_TOPICS)
// =============================================================================

// Chiều nhận (ESP32 Subscribe nhận lệnh)
#define SORTIX_TOPIC_VISION                 "sorter/sorter_01/vision"
#define SORTIX_TOPIC_CONTROL                "sorter/sorter_01/control"
#define SORTIX_TOPIC_ESTOP                  "conveyor/safety/estop"
#define SORTIX_TOPIC_ESTOP_RELEASE          "conveyor/safety/estop/release"
#define SORTIX_TOPIC_CONFIG_SET             "sorter/sorter_01/config/set"

// Chiều phát (ESP32 Publish gửi dữ liệu)
#define SORTIX_TOPIC_TELEMETRY              "sorter/sorter_01/telemetry"
#define SORTIX_TOPIC_STATUS                 "sorter/sorter_01/status"
#define SORTIX_TOPIC_CONFIG_STATUS          "sorter/sorter_01/config/status"
#define SORTIX_TOPIC_CLASSIFICATION         "sorter/sorter_01/classification"
#define SORTIX_TOPIC_HEARTBEAT              "conveyor/heartbeat"
#define SORTIX_TOPIC_JAM                    "conveyor/sensor/jam"
#define SORTIX_TOPIC_TEMP_WARN              "conveyor/telemetry/temp"
#define SORTIX_TOPIC_ESTOP_ACK              "conveyor/safety/estop/ack"

// Khóa bí mật chung (shared secret) được cấu hình qua cổng Captive Portal và phải khớp với
// biến SORTIX_CONTROL_SECRET trên Dashboard/backend.
#define SORTIX_CONTROL_SECRET               "sortix_sec_control_2026"

// Thông tin Wi-Fi Station (Kết nối vào Hotspot phát từ Laptop / Điện thoại 4G)
// ESP32-C5 chỉ hoạt động ở chế độ Station (STA)
#define DEFAULT_WIFI_SSID                   "HIHI"            // Tên điểm phát sóng Wi-Fi (SSID)
#define DEFAULT_WIFI_PASS                   "hocdegiau"                  // Mật khẩu Wi-Fi (để trống "" nếu không cài mật khẩu)
#define DEFAULT_MQTT_BROKER_URI             "mqtts://broker.emqx.io:8883"                // Địa chỉ máy chủ MQTT Broker
#define DEFAULT_HTTP_TELEMETRY_URL          "http://192.168.137.1:5000/api/telemetry"    // IP mặc định của Laptop khi bật Windows Mobile Hotspot

// =============================================================================
// Thông số cơ khí & Hiệu chuẩn theo cấu hình thực tế
// =============================================================================
// Động cơ 65 vòng/phút (65 RPM), tỉ lệ truyền với băng chuyền 1:1, puly 20 răng.
// Dùng Encoder 2 pha (GPIO1, GPIO4) để đếm xung xác định quãng đường đi được.
#define MOTOR_NOMINAL_RPM                   65.0f       // Tốc độ động cơ 65 vòng/phút
#define BELT_TRANSMISSION_RATIO             1.0f        // Tỉ lệ truyền với băng chuyền 1:1
#define PULLEY_TEETH_COUNT                  20          // Puly 20 răng
#define CALIB_ENCODER_COUNTS_PER_REV        3960
#define CALIB_ENCODER_COUNTS_PER_MM         63.0f       // Hệ số xung trên mỗi mm hành trình di chuyển của băng tải (hiệu chuẩn thực tế x4)
#define CONVEYOR_NOMINAL_SPEED_PCT          60          // Thang đo 0-100 theo chuẩn quy định của TelemetrySchema

#define DIST_S1_TO_CAMERA_MM                100.0f      // Khoảng cách từ cảm biến vật cản S1 đến buồng quét DroidCam (10cm)
#define CAMERA_SCAN_PAUSE_MS                2000        // Thời gian dừng băng tải tại buồng quét để DroidCam quét vật phẩm (2s)
#define DIST_S1_TO_SERVO1_MM                250.0f      // Khoảng cách từ cảm biến vật cản S1 đến Servo 1 - Ô chứa 1 (25cm)
#define DIST_S1_TO_SERVO2_MM                400.0f      // Khoảng cách từ cảm biến vật cản S1 đến Servo 2 - Ô chứa 2 (40cm)
#define DIST_S1_TO_END_MM                   650.0f      // Khoảng cách từ cảm biến vật cản S1 đến Ô chứa 3 - cuối băng tải (65cm)
#define SERVO_TRIGGER_WINDOW_MM             15.0f       // Dung sai khoảng cách (cửa sổ kích hoạt) quanh vị trí mục tiêu để gạt

// Thời gian chờ & Khoảng thời gian giám sát an toàn
#define VISION_TIMEOUT_MS                   2000        // Thời gian dừng quét vật phẩm bằng DroidCam tại buồng quét (2s)
#define AUTO_STANDBY_TIMEOUT_MS             15000       // Thời gian không có vật cản để tự động dừng băng tải về trạng thái ngủ (15s)
#define LDR_DARK_LEVEL                      1           // Mức logic chân DO cảm biến quang trở khi thiếu sáng: 1 = Tối (BẬT LED), 0 = Sáng (TẮT LED)
#define JAM_STARTUP_GRACE_MS                1000        // Bỏ qua kiểm tra kẹt trong giai đoạn khởi động tăng tốc động cơ ban đầu
#define JAM_DETECTION_WINDOW_MS             1800        // Nếu không có xung encoder trong 1.8s khi động cơ đang chạy -> Báo kẹt (JAM)
#define CPU_TEMP_ALERT_THRESHOLD_C          75.0f       // Ngưỡng nhiệt độ cảnh báo quá nhiệt trên chip SoC (độ C)
#define TELEMETRY_PUBLISH_INTERVAL_MS       1500        // Chu kỳ gửi dữ liệu đo đạc từ xa qua MQTT (1.5s)
#define HEARTBEAT_INTERVAL_MS               2000        // Chu kỳ phát tin hiệu nhịp tim (heartbeat) qua MQTT (2s)
#define HTTP_TELEMETRY_INTERVAL_MS          5000        // Chu kỳ gửi dữ liệu HTTP POST telemetry (5s)

#define MAX_TRACKED_ITEMS                   15          // Số lượng vật thể tối đa theo dõi đồng thời trong hàng đợi vòng FIFO
