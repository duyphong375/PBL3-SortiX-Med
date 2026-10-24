#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_attr.h"

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
 * @brief Get the encoder count reported by the configured PCNT unit.
 *        Thread-safe and ISR-safe.
 * @return Signed pulse count, widened to int64_t for position calculations.
 */
int64_t encoder_get_count(void);

/** Read count while preserving PCNT errors for safety-sensitive callers. */
esp_err_t encoder_get_count_checked(int64_t *count);

/** Fast count snapshot for IRAM GPIO handlers only. */
int64_t IRAM_ATTR encoder_get_count_isr(void);

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
