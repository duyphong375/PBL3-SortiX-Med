#include "state_machine.h"
#include "motor_control.h"
#include "servo.h"
#include "led_indicator.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

extern bool sorting_engine_has_unresolved_items(void);
extern esp_err_t sorting_engine_clear_unresolved(void);
extern void sorting_engine_reset(void);

static const char *TAG = "STATE_MACHINE";

static system_state_t s_state = SYSTEM_STATE_IDLE;
static bool s_mcu_awake = false;
static SemaphoreHandle_t s_state_mutex = NULL;
static state_change_cb_t s_cb = NULL;
static void *s_cb_user_ctx = NULL;

const char *state_machine_get_state_str(system_state_t state)
{
    switch (state) {
        case SYSTEM_STATE_IDLE:     return "IDLE";
        case SYSTEM_STATE_RUNNING:  return "RUNNING";
        case SYSTEM_STATE_STOPPED:  return "STOPPED";
        case SYSTEM_STATE_JAMMED:   return "JAMMED";
        case SYSTEM_STATE_ESTOP:    return "ESTOPPED";
        default:                    return "UNKNOWN";
    }
}

esp_err_t state_machine_init(void)
{
    if (s_state_mutex == NULL) {
        s_state_mutex = xSemaphoreCreateMutex();
        if (s_state_mutex == NULL) return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_state = SYSTEM_STATE_IDLE; // Luôn khởi động mặc định ở trạng thái an toàn IDLE
    s_mcu_awake = false;         // Khi mới cắm nguồn: MCU ở trạng thái ngủ (chờ nút nhấn GPIO23)
    esp_err_t motor_ret = motor_control_stop();
    servo_emergency_stop(); // Tắt toàn bộ xung PWM servo
    led_indicator_set_mode(LED_MODE_IDLE); // Tắt đèn LED và chưa kích hoạt LDR
    xSemaphoreGive(s_state_mutex);
    if (motor_ret != ESP_OK) return motor_ret;

    ESP_LOGI(TAG, "State Machine initialized in safe sleep state: IDLE (mcu_awake=false)");
    return ESP_OK;
}

esp_err_t state_machine_process_event(system_event_t event)
{
    if (s_state_mutex == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = ESP_OK;
    system_state_t old_state, new_state;

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    old_state = s_state;
    new_state = old_state;

    switch (event) {
        case SYSTEM_EVENT_START:
            if (old_state == SYSTEM_STATE_ESTOP) {
                ESP_LOGW(TAG, "START rejected: System is in EMERGENCY STOP! Admin unlock required.");
                ret = ESP_ERR_INVALID_STATE;
            } else if (old_state == SYSTEM_STATE_JAMMED) {
                ESP_LOGW(TAG, "START rejected: System is JAMMED! Release/clear jam first.");
                ret = ESP_ERR_INVALID_STATE;
            } else if (sorting_engine_has_unresolved_items() && old_state == SYSTEM_STATE_STOPPED && !s_mcu_awake) {
                ESP_LOGW(TAG, "START rejected: Unresolved held/timeout items on conveyor! Clear belt before restarting.");
                ret = ESP_ERR_INVALID_STATE;
            } else if (old_state == SYSTEM_STATE_IDLE || old_state == SYSTEM_STATE_STOPPED) {
                sorting_engine_reset();
                ret = motor_control_forward();
                if (ret == ESP_OK) {
                    s_mcu_awake = true;
                    new_state = SYSTEM_STATE_RUNNING;
                    led_indicator_set_mode(LED_MODE_RUNNING);
                    ESP_LOGI(TAG, "Transition [%s] -> [RUNNING]: Motor started (mcu_awake=true)",
                             state_machine_get_state_str(old_state));
                }
            } else {
                ESP_LOGD(TAG, "START ignored: Already RUNNING");
            }
            break;

        case SYSTEM_EVENT_STOP:
            if (old_state == SYSTEM_STATE_ESTOP || old_state == SYSTEM_STATE_JAMMED) {
                ESP_LOGW(TAG, "STOP does not release the latched emergency state");
                ret = ESP_ERR_INVALID_STATE;
            } else {
                ret = motor_control_stop();
                if (ret == ESP_OK) {
                    esp_err_t servo_ret = servo_emergency_stop();
                    if (!sorting_engine_has_unresolved_items()) {
                        sorting_engine_reset();
                    }
                    s_mcu_awake = false; // Tắt toàn bộ hệ thống về trạng thái ngủ (khóa S1, LDR, Servo, Motor)
                    if (old_state == SYSTEM_STATE_RUNNING || old_state == SYSTEM_STATE_IDLE) {
                        new_state = SYSTEM_STATE_STOPPED;
                        ESP_LOGI(TAG, "Transition [%s] -> [STOPPED]: All actuators & sensors OFF (mcu_awake=false)",
                                 state_machine_get_state_str(old_state));
                    }
                    led_indicator_set_mode(LED_MODE_IDLE);
                    if (servo_ret != ESP_OK) ret = servo_ret;
                }
            }
            break;

        case SYSTEM_EVENT_STANDBY:
            if (old_state == SYSTEM_STATE_RUNNING) {
                ret = motor_control_stop();
                if (ret == ESP_OK) {
                    esp_err_t servo_ret = servo_emergency_stop();
                    sorting_engine_reset();
                    // LƯU Ý QUAN TRỌNG: Giữ s_mcu_awake = true!
                    // Chỉ tạm dừng băng tải sau 15s không có vật cản; cảm biến S1 và LDR vẫn thức
                    // để ngay khi có vật cản mới đi vào S1 thì băng tải tự động chạy lại ngay!
                    new_state = SYSTEM_STATE_IDLE;
                    led_indicator_set_mode(LED_MODE_RUNNING);
                    ESP_LOGI(TAG, "Transition [RUNNING] -> [IDLE]: 15s Auto Standby (Conveyor paused, waiting for next obstacle at S1)");
                    if (servo_ret != ESP_OK) ret = servo_ret;
                }
            } else {
                ESP_LOGD(TAG, "STANDBY ignored: Not currently RUNNING");
            }
            break;

        case SYSTEM_EVENT_JAM_DETECTED:
            s_mcu_awake = false;
            new_state = SYSTEM_STATE_JAMMED;
            ret = motor_control_brake(); // Kích hoạt hãm động năng khi phát hiện kẹt cơ khí
            {
                esp_err_t servo_ret = servo_emergency_stop();
                if (ret == ESP_OK) ret = servo_ret;
            }
            led_indicator_set_mode(LED_MODE_JAMMED);
            ESP_LOGE(TAG, "EMERGENCY JAM! Transition [%s] -> [JAMMED]: motor brake and servo stop commands issued",
                     state_machine_get_state_str(old_state));
            break;

        case SYSTEM_EVENT_ESTOP:
            s_mcu_awake = false;
            new_state = SYSTEM_STATE_ESTOP;
            ret = motor_control_brake(); // Kích hoạt hãm động năng khi dừng khẩn cấp
            {
                esp_err_t servo_ret = servo_emergency_stop();
                if (ret == ESP_OK) ret = servo_ret;
            }
            led_indicator_set_mode(LED_MODE_JAMMED);
            ESP_LOGE(TAG, "EMERGENCY STOP! Transition [%s] -> [ESTOPPED]: motor brake and servo signal disable commands issued",
                     state_machine_get_state_str(old_state));
            break;

        case SYSTEM_EVENT_ESTOP_RELEASE:
            // KHÔNG tự động xóa các vật thể chưa được xử lý trên băng tải!
            // Các vật thể vẫn được lưu trong hàng đợi và yêu cầu người vận hành kiểm tra dọn sạch trước khi cho phép START lại.
            if (old_state == SYSTEM_STATE_ESTOP || old_state == SYSTEM_STATE_JAMMED || old_state == SYSTEM_STATE_STOPPED) {
                ret = motor_control_stop();
                if (ret == ESP_OK) {
                    new_state = SYSTEM_STATE_IDLE; // Chuyển an toàn về IDLE, TUYỆT ĐỐI KHÔNG tự khởi động động cơ
                    led_indicator_set_mode(LED_MODE_IDLE);
                    ESP_LOGI(TAG, "Safety unlocked [%s] -> [IDLE]. System ready for START command.",
                             state_machine_get_state_str(old_state));
                } else {
                    ESP_LOGE(TAG, "Safety release rejected because motor stop pins could not be set: %s",
                             esp_err_to_name(ret));
                }
            } else {
                ESP_LOGI(TAG, "ESTOP_RELEASE received in state %s. Maintaining IDLE safe state.",
                         state_machine_get_state_str(old_state));
                new_state = SYSTEM_STATE_IDLE;
                led_indicator_set_mode(LED_MODE_IDLE);
            }
            break;

        default:
            ret = ESP_ERR_INVALID_ARG;
            break;
    }

    s_state = new_state;
    xSemaphoreGive(s_state_mutex);

    if (new_state != old_state && s_cb != NULL) {
        s_cb(old_state, new_state, s_cb_user_ctx);
    }

    return ret;
}

system_state_t state_machine_get_state(void)
{
    if (s_state_mutex == NULL) return s_state;
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    system_state_t state = s_state;
    xSemaphoreGive(s_state_mutex);
    return state;
}

bool state_machine_is_awake(void)
{
    if (s_state_mutex == NULL) return s_mcu_awake;
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    bool awake = s_mcu_awake;
    xSemaphoreGive(s_state_mutex);
    return awake;
}

void state_machine_register_callback(state_change_cb_t cb, void *user_ctx)
{
    s_cb = cb;
    s_cb_user_ctx = user_ctx;
}
