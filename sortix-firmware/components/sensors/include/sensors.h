#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int64_t encoder_count;      // Latch of PCNT encoder at detection edge
    int64_t timestamp_us;       // System microsecond timestamp at detection
} s1_event_t;

/**
 * @brief Initialize IR Obstacle Sensor (S1 on GPIO0) and LDR Sensor (GPIO5).
 *        Attaches falling edge interrupt to GPIO0 and routes captured events to queue.
 * @param event_queue Queue handle to receive s1_event_t structs.
 * @return ESP_OK on success.
 */
esp_err_t sensors_init(QueueHandle_t event_queue);

/**
 * @brief Get instantaneous physical reading of IR Sensor S1.
 * @return true if an obstacle is currently blocking S1 (pin is LOW, active low).
 */
bool sensors_is_s1_blocked(void);

/**
 * @brief Get instantaneous reading of LDR light sensor (GPIO5).
 * @return 0 or 1 logical level.
 */
int sensors_get_ldr_level(void);

/** Return and clear the flag raised when the S1 ISR queue was full. */
bool sensors_take_s1_queue_overflow(void);

#ifdef __cplusplus
}
#endif
