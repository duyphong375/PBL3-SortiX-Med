#include "state_machine.h"
#include "motor_control.h"
#include "led_indicator.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "STATE_MACHINE";

static system_state_t s_state = SYSTEM_STATE_IDLE;
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
    s_state = SYSTEM_STATE_IDLE; // Always boots into IDLE
    motor_control_stop();
    led_indicator_set_mode(LED_MODE_IDLE);
    xSemaphoreGive(s_state_mutex);

    ESP_LOGI(TAG, "State Machine initialized in safe state: IDLE");
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
            if (old_state == SYSTEM_STATE_IDLE || old_state == SYSTEM_STATE_STOPPED) {
                new_state = SYSTEM_STATE_RUNNING;
                motor_control_forward();
                led_indicator_set_mode(LED_MODE_RUNNING);
                ESP_LOGI(TAG, "Transition [%s] -> [RUNNING]: Motor started",
                         state_machine_get_state_str(old_state));
            } else if (old_state == SYSTEM_STATE_JAMMED) {
                ESP_LOGW(TAG, "START rejected: System is JAMMED! Release/clear jam first.");
                ret = ESP_ERR_INVALID_STATE;
            } else {
                ESP_LOGD(TAG, "START ignored: Already RUNNING");
            }
            break;

        case SYSTEM_EVENT_STOP:
            if (old_state == SYSTEM_STATE_RUNNING) {
                new_state = SYSTEM_STATE_STOPPED;
                motor_control_stop();
                led_indicator_set_mode(LED_MODE_IDLE);
                ESP_LOGI(TAG, "Transition [RUNNING] -> [STOPPED]: Motor stopped");
            } else {
                ESP_LOGD(TAG, "STOP ignored: Not currently RUNNING");
            }
            break;

        case SYSTEM_EVENT_JAM_DETECTED:
            new_state = SYSTEM_STATE_JAMMED;
            motor_control_brake(); // Dynamic brake on jam
            led_indicator_set_mode(LED_MODE_JAMMED);
            ESP_LOGE(TAG, "EMERGENCY JAM! Transition [%s] -> [JAMMED]: Motor dynamically braked!",
                     state_machine_get_state_str(old_state));
            break;

        case SYSTEM_EVENT_ESTOP_RELEASE:
            if (old_state == SYSTEM_STATE_JAMMED || old_state == SYSTEM_STATE_STOPPED) {
                new_state = SYSTEM_STATE_IDLE; // Safely transitions to IDLE ONLY, DOES NOT start motor
                motor_control_stop();
                led_indicator_set_mode(LED_MODE_IDLE);
                ESP_LOGI(TAG, "Safety unlocked [%s] -> [IDLE]. System ready for START command.",
                         state_machine_get_state_str(old_state));
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
    return s_state;
}

void state_machine_register_callback(state_change_cb_t cb, void *user_ctx)
{
    s_cb = cb;
    s_cb_user_ctx = user_ctx;
}
