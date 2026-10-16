#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_IDLE,          // Slow blink (1 Hz): 500ms ON / 500ms OFF
    LED_MODE_RUNNING,       // Steady ON
    LED_MODE_JAMMED,        // Fast blink (5 Hz): 100ms ON / 100ms OFF
    LED_MODE_ERROR          // Fast blink (5 Hz): 100ms ON / 100ms OFF
} led_mode_t;

/**
 * @brief Initialize Status LED on GPIO10.
 *        Starts 100ms periodic software timer for non-blocking pattern generation.
 * @return ESP_OK on success.
 */
esp_err_t led_indicator_init(void);

/**
 * @brief Set current LED display pattern.
 *        Thread-safe.
 * @param mode Target mode.
 */
void led_indicator_set_mode(led_mode_t mode);

/**
 * @brief Get current LED display mode.
 */
led_mode_t led_indicator_get_mode(void);

#ifdef __cplusplus
}
#endif
