#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "app_config.h"
#include "version.h"
#include "pin_definitions.h"
#include "nvs_config.h"
#include "motor_control.h"
#include "encoder.h"
#include "servo.h"
#include "sensors.h"
#include "button.h"
#include "led_indicator.h"
#include "state_machine.h"
#include "safety_monitor.h"
#include "sorting_engine.h"
#include "wifi_manager.h"
#include "mqtt_handler.h"
#include "http_telemetry.h"
#include "captive_portal.h"

static const char *TAG = "MAIN_APP";

static QueueHandle_t s_s1_queue = NULL;

// Hàm Callback: Nút nhấn vật lý GPIO23 được bấm (Cơ chế Toggle đa năng: Bấm 1 = CHẠY, Bấm 2 = DỪNG)
// Phần mềm lọc rung debounce 30ms, không cần mạch lọc RC phần cứng
static void on_start_button_pressed(void *user_ctx)
{
    static int64_t s_last_toggle_us = 0;
    int64_t now_us = esp_timer_get_time();
    const int64_t toggle_cooldown_us = 300000; // Khóa 300ms chống nảy kép

    if ((now_us - s_last_toggle_us) < toggle_cooldown_us) {
        return;
    }
    s_last_toggle_us = now_us;

    if (state_machine_is_awake()) {
        ESP_LOGI(TAG, "Hardware Button GPIO23 pressed -> Toggling MCU to SLEEP/OFF (STOPPED)");
        state_machine_process_event(SYSTEM_EVENT_STOP);
    } else {
        ESP_LOGI(TAG, "Hardware Button GPIO23 pressed -> Waking up MCU to START (RUNNING)");
        if (s_s1_queue != NULL) {
            xQueueReset(s_s1_queue);
        }
        (void)sensors_take_s1_queue_overflow();
        sensors_rearm_s1();
        state_machine_process_event(SYSTEM_EVENT_START);
    }
}

// Hàm Callback: Chuyển đổi trạng thái trong máy trạng thái FSM
static void on_state_changed(system_state_t old_state, system_state_t new_state, void *user_ctx)
{
    const char *state_str = state_machine_get_state_str(new_state);
    ESP_LOGI(TAG, "System state changed: %s -> %s",
             state_machine_get_state_str(old_state), state_str);
    mqtt_handler_publish_status(state_str);
    mqtt_handler_publish_telemetry(); // Đồng bộ ngay tức thì sang Dashboard Web (<50ms)
}

// Hàm Callback: Hoàn thành phân loại vật thể trong động cơ phân loại
static void on_item_classified(const tracked_item_t *item, void *user_ctx)
{
    ESP_LOGI(TAG, "Publishing classification record: ID #%lu, Product '%s', Brand '%s' -> Bin %d (%s)",
             item->id, item->product_id, item->brand_id, item->actual_bin, item->status);
    mqtt_handler_publish_classification(item);
}

// Hàm Callback: Giám sát an toàn phát hiện kẹt cơ cấu/encoder dừng quay
static void on_jam_detected(float duration_seconds, void *user_ctx)
{
    ESP_LOGE(TAG, "Encoder stall jam confirmed (duration: %.2f s) -> Publishing jam alert", duration_seconds);
    mqtt_handler_publish_jam(duration_seconds);
}

// Hàm Callback: Cảnh báo quá nhiệt chip SoC từ giám sát an toàn
static void on_temp_warning(float current_temp, float threshold_temp, void *user_ctx)
{
    ESP_LOGW(TAG, "SoC Temperature warning: %.1f °C -> Publishing warning alert", current_temp);
    mqtt_handler_publish_temp_warning(current_temp, threshold_temp);
}

// Task 1: task_conveyor (Độ ưu tiên 6)
// Điều khiển động cơ L298N và giám sát kẹt băng tải an toàn theo xung encoder
static void task_conveyor(void *pvParameters)
{
    ESP_LOGI(TAG, "task_conveyor started (Priority %d)", uxTaskPriorityGet(NULL));
    const TickType_t delay_ticks = pdMS_TO_TICKS(20);

    while (1) {
        // Kiểm tra an toàn và phát hiện kẹt liên tục
        safety_monitor_check();
        vTaskDelay(delay_ticks);
    }
}

// Task 2: task_sorting (Độ ưu tiên 5)
// Theo dõi vị trí vật thể trên băng tải, ghép nối kết quả AI, kích hoạt servo gạt
// và điều phối tự động: Tự động đánh thức (Auto Wake-up) / Tự động ngủ (Auto Standby)
static void task_sorting(void *pvParameters)
{
    ESP_LOGI(TAG, "task_sorting started (Priority %d)", uxTaskPriorityGet(NULL));
    s1_event_t s1_evt;
    int64_t last_active_time_us = esp_timer_get_time();
    system_state_t prev_state = SYSTEM_STATE_IDLE;

    while (1) {
        system_state_t cur_state = state_machine_get_state();

        // Nếu vừa chuyển sang RUNNING từ trạng thái khác -> Reset mốc thời gian hoạt động
        if (cur_state == SYSTEM_STATE_RUNNING && prev_state != SYSTEM_STATE_RUNNING) {
            last_active_time_us = esp_timer_get_time();
        }
        prev_state = cur_state;

        // 1. Nhận sự kiện từ cảm biến vật cản S1
        while (xQueueReceive(s_s1_queue, &s1_evt, 0) == pdTRUE) {
            cur_state = state_machine_get_state();

            if (!state_machine_is_awake()) {
                // Khi MCU ở trạng thái Ngủ hoàn toàn (mới cắm nguồn hoặc đã bấm GPIO23 tắt):
                // Bỏ qua mọi tín hiệu từ cảm biến S1
                ESP_LOGD(TAG, "S1 trigger ignored because MCU is sleeping/OFF in [%s]",
                         state_machine_get_state_str(cur_state));
                continue;
            }

            // Nếu MCU đã được bật (awake = true) nhưng băng tải đang tạm dừng sau 15s không có vật cản (IDLE):
            // Ngay khi có vật cản chạm S1 -> TỰ ĐỘNG KHỞI ĐỘNG LẠI BĂNG TẢI NGAY LẬP TỨC!
            if (cur_state == SYSTEM_STATE_IDLE) {
                ESP_LOGI(TAG, "Obstacle detected at S1 during 15s Standby -> Auto-restarting conveyor!");
                state_machine_process_event(SYSTEM_EVENT_START);
                cur_state = state_machine_get_state();
                prev_state = cur_state;
            }

            if (cur_state == SYSTEM_STATE_RUNNING) {
                // Đưa vật thể vào pipeline theo dõi của sorting engine
                sorting_engine_on_s1_entry(encoder_get_count(), s1_evt.timestamp_us);
                last_active_time_us = esp_timer_get_time();
            }
        }

        // Trường hợp vật cản đã đặt đè sẵn lên mắt S1 trong lúc băng tải đang tạm dừng 15s (IDLE):
        cur_state = state_machine_get_state();
        if (cur_state == SYSTEM_STATE_IDLE && state_machine_is_awake() && sensors_is_s1_blocked()) {
            ESP_LOGI(TAG, "Obstacle blocking S1 during 15s Standby -> Auto-restarting conveyor!");
            state_machine_process_event(SYSTEM_EVENT_START);
            cur_state = state_machine_get_state();
            prev_state = cur_state;
            if (cur_state == SYSTEM_STATE_RUNNING && sorting_engine_is_line_empty()) {
                sorting_engine_on_s1_entry(encoder_get_count(), esp_timer_get_time());
                last_active_time_us = esp_timer_get_time();
            }
        }

        if (sensors_take_s1_queue_overflow() && cur_state == SYSTEM_STATE_RUNNING) {
            ESP_LOGE(TAG, "S1 event queue overflow: stopping conveyor and requiring operator clearance");
            sorting_engine_note_tracking_overflow();
        }

        // 3. Cập nhật vị trí di chuyển theo encoder, kiểm tra cửa sổ kích hoạt servo và timeout nhận diện AI
        sorting_engine_update();

        // 4. Quản lý Chế độ ngủ tự động (Auto Standby / Auto-Sleep - 15s)
        cur_state = state_machine_get_state();
        if (cur_state == SYSTEM_STATE_RUNNING) {
            // Nếu băng tải vẫn còn vật thể đang trôi, S1 đang bị che, hoặc Servo đang bận hoạt động (tự động hoặc test tay từ Web):
            // Luôn giữ mốc thời gian hoạt động mới nhất (không tính thời gian gạt servo vào thời gian rảnh rỗi)
            if (!sorting_engine_is_line_empty() || sensors_is_s1_blocked() ||
                servo_is_busy(SERVO_ID_1) || servo_is_busy(SERVO_ID_2)) {
                last_active_time_us = esp_timer_get_time();
            } else {
                // Băng tải đã gạt hết đồ, S1 thông thoáng và toàn bộ tay gạt đã nghỉ: Kiểm tra thời gian không hoạt động (15s)
                int64_t idle_time_ms = (esp_timer_get_time() - last_active_time_us) / 1000;
                if (idle_time_ms >= AUTO_STANDBY_TIMEOUT_MS) {
                    ESP_LOGI(TAG, "Auto Standby: Inactive for %lld ms with clear line -> Entering IDLE safe sleep",
                             idle_time_ms);
                    state_machine_process_event(SYSTEM_EVENT_STANDBY);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(15));
    }
}

// Task 3: task_mqtt (Độ ưu tiên 4)
// Phát dữ liệu đo đạc (telemetry) và gói tin nhịp tim (heartbeat) định kỳ lên SortiX-Dashboard
static void task_mqtt(void *pvParameters)
{
    ESP_LOGI(TAG, "task_mqtt started (Priority %d)", uxTaskPriorityGet(NULL));

    TickType_t last_telemetry_tick = xTaskGetTickCount();
    TickType_t last_heartbeat_tick = xTaskGetTickCount();

    const TickType_t telemetry_period = pdMS_TO_TICKS(TELEMETRY_PUBLISH_INTERVAL_MS);
    const TickType_t heartbeat_period = pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS);

    while (1) {
        TickType_t now = xTaskGetTickCount();

        if (mqtt_handler_is_connected()) {
            if ((now - last_telemetry_tick) >= telemetry_period) {
                last_telemetry_tick = now;
                mqtt_handler_publish_telemetry();
            }

            if ((now - last_heartbeat_tick) >= heartbeat_period) {
                last_heartbeat_tick = now;
                mqtt_handler_publish_heartbeat();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// Task 4: task_http_tele (Độ ưu tiên 2)
// Gửi HTTP POST telemetry định kỳ mỗi 5s tới máy chủ backend Node.js (cổng 5000)
static void task_http_tele(void *pvParameters)
{
    ESP_LOGI(TAG, "task_http_tele started (Priority %d)", uxTaskPriorityGet(NULL));

    // Chờ cho đến khi Wi-Fi kết nối thành công
    while (!wifi_manager_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    while (1) {
        if (wifi_manager_is_connected()) {
            http_telemetry_send_post();
        }
        vTaskDelay(pdMS_TO_TICKS(HTTP_TELEMETRY_INTERVAL_MS));
    }
}

// Task 5: task_safety (Độ ưu tiên 5)
// Giám sát sức khỏe hệ thống và trạng thái kết nối Wi-Fi Station
static void task_safety(void *pvParameters)
{
    ESP_LOGI(TAG, "task_safety started (Priority %d)", uxTaskPriorityGet(NULL));

    while (1) {
        // Giám sát trạng thái Wi-Fi Station (tự động kết nối lại khi có Hotspot)
        static int disconnect_counter = 0;
        if (!wifi_manager_is_connected()) {
            disconnect_counter++;
            if (disconnect_counter % 5 == 0) {
                ESP_LOGW(TAG, "Wi-Fi Station is waiting for Hotspot connection (%ds)...", disconnect_counter);
            }
        } else {
            disconnect_counter = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void init_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP for ISO8601 timestamps");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();
}

void app_main(void)
{
    ESP_LOGI(TAG, "=========================================================");
    ESP_LOGI(TAG, "   SortiX Conveyor Firmware - v%s", SORTIX_FW_VERSION_STRING);
    ESP_LOGI(TAG, "   Target: %s (%s)", SORTIX_TARGET_CHIP, SORTIX_BOARD_NAME);
    ESP_LOGI(TAG, "=========================================================");

    // 1. Khởi tạo bộ nhớ lưu trữ bất biến (NVS)
    ESP_ERROR_CHECK(nvs_config_init());

    // 2. Khởi tạo LED trạng thái (GPIO10)
    ESP_ERROR_CHECK(led_indicator_init());

    // 3. Khởi tạo điều khiển động cơ (GPIO8 IN1, GPIO9 IN2)
    ESP_ERROR_CHECK(motor_control_init());

    // 4. Khởi tạo Servo MG996R non-blocking (GPIO6 S1, GPIO7 S2)
    ESP_ERROR_CHECK(servo_init());

    // 5. Khởi tạo máy trạng thái FSM (Luôn khởi động an toàn ở trạng thái IDLE)
    ESP_ERROR_CHECK(state_machine_init());
    state_machine_register_callback(on_state_changed, NULL);

    // 6. Khởi tạo Encoder quay PCNT (GPIO1 Phase A, GPIO4 Phase B)
    ESP_ERROR_CHECK(encoder_init());
    const sortix_sys_config_t *sys_cfg = nvs_config_get();
    if (sys_cfg != NULL && sys_cfg->counts_per_mm > 0.001f) {
        encoder_set_counts_per_mm(sys_cfg->counts_per_mm);
        ESP_LOGI(TAG, "Applied calibrated encoder resolution: %.2f counts/mm", sys_cfg->counts_per_mm);
    }

    // 7. Tạo hàng đợi FreeRTOS cho sự kiện cảm biến quang S1
    s_s1_queue = xQueueCreate(16, sizeof(s1_event_t));
    if (s_s1_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create S1 event queue!");
        return;
    }

    // 8. Khởi tạo cảm biến (IR S1 trên GPIO3, LDR DO trên GPIO5)
    ESP_ERROR_CHECK(sensors_init(s_s1_queue));

    // 9. Khởi tạo nút bấm Start (GPIO23 với lọc nhiễu debounce 20ms)
    ESP_ERROR_CHECK(button_init(on_start_button_pressed, NULL));

    // 10. Khởi tạo giám sát an toàn Safety Monitor (Kẹt encoder & cảm biến nhiệt độ SoC)
    ESP_ERROR_CHECK(safety_monitor_init());
    safety_monitor_set_jam_callback(on_jam_detected, NULL);
    safety_monitor_set_temp_callback(on_temp_warning, NULL);

    // 11. Khởi tạo bộ máy phân loại Sorting Engine (Tracking Ring Buffer & định tuyến động)
    ESP_ERROR_CHECK(sorting_engine_init());
    sorting_engine_set_classification_callback(on_item_classified, NULL);

    // 12. Khởi tạo Wi-Fi Station
    ESP_ERROR_CHECK(wifi_manager_init());
    init_sntp();

    // 13. Khởi tạo MQTT Client (Giao thức SortiX-Dashboard)
    esp_err_t mqtt_ret = mqtt_handler_init();
    if (mqtt_ret != ESP_OK) {
        ESP_LOGW(TAG, "MQTT client not initialized (%s); will auto-connect when Wi-Fi Hotspot is available",
                 esp_err_to_name(mqtt_ret));
    }

    // 14. Khởi tạo HTTP Telemetry Client (Backend cổng 5000)
    ESP_ERROR_CHECK(http_telemetry_init());

    // 15. Tạo các tác vụ FreeRTOS
    xTaskCreate(task_conveyor, "task_conveyor", 4096, NULL, 6, NULL);
    xTaskCreate(task_sorting,  "task_sorting",  4096, NULL, 5, NULL);
    xTaskCreate(task_mqtt,     "task_mqtt",     4096, NULL, 4, NULL);
    xTaskCreate(task_http_tele,"task_http_tele",4096, NULL, 2, NULL);
    xTaskCreate(task_safety,   "task_safety",   3072, NULL, 5, NULL);

    ESP_LOGI(TAG, "Firmware boot sequence complete. System in IDLE safe state.");
}
