#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize rotary encoder with New PCNT Driver (GPIO1 Phase A, GPIO4 Phase B).
 *        Sets up quadrature x4 decoding, glitch filter, and 64-bit overflow accumulator.
 * @return ESP_OK on success.
 */
esp_err_t encoder_init(void);

/**
 * @brief Get cumulative 64-bit encoder counts.
 *        Thread-safe and ISR-safe.
 * @return Signed 64-bit pulse count.
 */
int64_t encoder_get_count(void);

/**
 * @brief Reset encoder count to zero.
 */
void encoder_reset_count(void);

/**
 * @brief Set calibration scale factor (counts per mm).
 * @param counts_per_mm Scale factor (default ~31.5f).
 */
void encoder_set_counts_per_mm(float counts_per_mm);

/**
 * @brief Get current calibration scale factor.
 */
float encoder_get_counts_per_mm(void);

/**
 * @brief Convert encoder counts to linear distance in mm.
 */
float encoder_counts_to_mm(int64_t counts);

/**
 * @brief Convert linear distance in mm to encoder counts.
 */
int64_t encoder_mm_to_counts(float mm);

#ifdef __cplusplus
}
#endif
