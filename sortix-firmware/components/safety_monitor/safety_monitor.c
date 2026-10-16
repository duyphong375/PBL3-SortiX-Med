#include "safety_monitor.h"
#include "encoder.h"
#include "motor_control.h"
#include "state_machine.h"
#include "driver/temperature_sensor.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "SAFETY_MONITOR";

#define JAM_STARTUP_GRACE_US        1000000 // 1.0s grace period after motor start
#define JAM_DETECTION_WINDOW_US     1800000 // 1.8s zero-count window while running -> JAM
#define TEMP_CHECK_INTERVAL_US      2000000 // Check temperature every 2.0s
#define TEMP_ALERT_COOLDOWN_US      10000000 // Cooldown 10s between repeated temp alerts
#define CPU_TEMP_ALERT_THRESHOLD    75.0f

static temperature_sensor_handle_t s_temp_sensor = NULL;
static float s_latest_temp = 40.0f;

static jam_alert_cb_t s_jam_cb = NULL;
static void *s_jam_user_ctx = NULL;

static temp_alert_cb_t s_temp_cb = NULL;
static void *s_temp_user_ctx = NULL;

static bool s_was_running = false;
static int64_t s_motor_start_time_us = 0;
static int64_t s_last_encoder_count = 0;
static int64_t s_last_movement_time_us = 0;
static int64_t s_last_temp_check_us = 0;
static int64_t s_last_temp_alert_us = 0;

esp_err_t safety_monitor_init(void)
{
    ESP_LOGI(TAG, "Initializing Safety Monitor (Internal SoC Temperature & Encoder Stall)");

    // Initialize On-Chip SoC Temperature Sensor
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
    esp_err_t ret = temperature_sensor_install(&temp_sensor_config, &s_temp_sensor);
    if (ret == ESP_OK) {
        ret = temperature_sensor_enable(s_temp_sensor);
        if (ret == ESP_OK) {
            temperature_sensor_get_celsius(s_temp_sensor, &s_latest_temp);
            ESP_LOGI(TAG, "SoC Temperature sensor enabled. Initial reading: %.1f °C", s_latest_temp);
        } else {
            ESP_LOGW(TAG, "Could not enable temperature sensor: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGW(TAG, "Could not install temperature sensor: %s", esp_err_to_name(ret));
    }

    s_was_running = false;
    return ESP_OK;
}

void safety_monitor_set_jam_callback(jam_alert_cb_t cb, void *user_ctx)
{
    s_jam_cb = cb;
    s_jam_user_ctx = user_ctx;
}

void safety_monitor_set_temp_callback(temp_alert_cb_t cb, void *user_ctx)
{
    s_temp_cb = cb;
    s_temp_user_ctx = user_ctx;
}

float safety_monitor_get_cpu_temp(void)
{
    return s_latest_temp;
}

void safety_monitor_check(void)
{
    int64_t now_us = esp_timer_get_time();

    // 1. Check SoC internal temperature every 2 seconds
    if (s_temp_sensor != NULL && (now_us - s_last_temp_check_us) >= TEMP_CHECK_INTERVAL_US) {
        s_last_temp_check_us = now_us;
        float current_temp = 0.0f;
        if (temperature_sensor_get_celsius(s_temp_sensor, &current_temp) == ESP_OK) {
            s_latest_temp = current_temp;
            if (current_temp >= CPU_TEMP_ALERT_THRESHOLD) {
                if ((now_us - s_last_temp_alert_us) >= TEMP_ALERT_COOLDOWN_US) {
                    s_last_temp_alert_us = now_us;
                    ESP_LOGW(TAG, "SoC Over-temperature WARNING: %.1f °C (Threshold: %.1f °C)",
                             current_temp, CPU_TEMP_ALERT_THRESHOLD);
                    if (s_temp_cb != NULL) {
                        s_temp_cb(current_temp, CPU_TEMP_ALERT_THRESHOLD, s_temp_user_ctx);
                    }
                }
            }
        }
    }

    // 2. Check Encoder Stall / Mechanical Jam
    bool is_running = motor_control_is_running();
    if (is_running) {
        if (!s_was_running) {
            // Motor just transitioned to running forward
            s_was_running = true;
            s_motor_start_time_us = now_us;
            s_last_encoder_count = encoder_get_count();
            s_last_movement_time_us = now_us;
            return;
        }

        // Check if outside initial startup grace period
        if ((now_us - s_motor_start_time_us) >= JAM_STARTUP_GRACE_US) {
            int64_t current_count = encoder_get_count();
            if (current_count != s_last_encoder_count) {
                // Pulse count is progressing normally
                s_last_encoder_count = current_count;
                s_last_movement_time_us = now_us;
            } else {
                // Pulse count has not changed
                int64_t stall_duration_us = now_us - s_last_movement_time_us;
                if (stall_duration_us >= JAM_DETECTION_WINDOW_US) {
                    float duration_sec = (float)stall_duration_us / 1000000.0f;
                    ESP_LOGE(TAG, "ENCODER STALL DETECTED! Conveyor stationary for %.2fs while motor running!",
                             duration_sec);

                    // Trigger state machine emergency jam
                    state_machine_process_event(SYSTEM_EVENT_JAM_DETECTED);
                    s_was_running = false;

                    if (s_jam_cb != NULL) {
                        s_jam_cb(duration_sec, s_jam_user_ctx);
                    }
                }
            }
        }
    } else {
        s_was_running = false;
    }
}
