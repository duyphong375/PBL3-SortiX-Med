#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*jam_alert_cb_t)(float duration_seconds, void *user_ctx);
typedef void (*temp_alert_cb_t)(float current_temp, float threshold_temp, void *user_ctx);

/**
 * @brief Initialize Safety Monitor subsystem:
 *        - On-chip temperature sensor (driver/temperature_sensor.h)
 *        - Encoder stall / mechanical jam detector
 * @return ESP_OK on success.
 */
esp_err_t safety_monitor_init(void);

/**
 * @brief Register callback for encoder stall mechanical jam events.
 */
void safety_monitor_set_jam_callback(jam_alert_cb_t cb, void *user_ctx);

/**
 * @brief Register callback for SoC over-temperature events.
 */
void safety_monitor_set_temp_callback(temp_alert_cb_t cb, void *user_ctx);

/**
 * @brief Get latest measured on-chip CPU temperature in Celsius.
 */
bool safety_monitor_get_cpu_temp(float *temperature_c);

/** Return a fresh encoder-derived conveyor speed sample in mm/s. */
bool safety_monitor_get_conveyor_speed_mm_s(float *speed_mm_s);

/**
 * @brief Periodic safety monitoring check routine.
 *        Called by safety task or FreeRTOS timer.
 */
void safety_monitor_check(void);

#ifdef __cplusplus
}
#endif
