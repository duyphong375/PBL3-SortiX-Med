#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SERVO_ID_1 = 0,     // Diverter 1: GPIO6 (Bin 1)
    SERVO_ID_2,         // Diverter 2: GPIO24 (Bin 2)
    SERVO_COUNT
} servo_id_t;

typedef enum {
    SERVO_STATE_IDLE = 0,
    SERVO_STATE_OPENING,
    SERVO_STATE_HOLDING,
    SERVO_STATE_CLOSING
} servo_fsm_state_t;

/**
 * @brief Initialize both MG996R servos via LEDC PWM 50Hz (GPIO6 and GPIO24).
 *        Sets up non-blocking timer-driven FSM. Both arms start in IDLE (retracted).
 * @return ESP_OK on success.
 */
esp_err_t servo_init(void);

/**
 * @brief Trigger a non-blocking divert cycle for the specified servo.
 *        Transitions FSM: IDLE -> OPENING -> HOLDING -> CLOSING -> IDLE.
 *        Does NOT block calling task!
 * @param id SERVO_ID_1 or SERVO_ID_2.
 * @return ESP_OK if triggered, ESP_ERR_INVALID_STATE if already in motion.
 */
esp_err_t servo_trigger(servo_id_t id);

/**
 * @brief Force immediate return to IDLE rest position for all servos.
 */
void servo_reset_all(void);

/** Disable PWM commands to all servos immediately for a latched safety stop. */
esp_err_t servo_emergency_stop(void);

/**
 * @brief Check if a servo is currently actively diverting (non-IDLE).
 */
bool servo_is_busy(servo_id_t id);

/**
 * @brief Get current FSM state of a servo.
 */
servo_fsm_state_t servo_get_state(servo_id_t id);

/**
 * @brief Configure pulse width parameters in microseconds.
 * @param rest_us Rest pulse width (default 1000us ~ 0 deg).
 * @param active_us Active pulse width (default 2000us ~ 90 deg).
 * @param hold_ms Duration to hold in active position before closing (default 600ms).
 */
void servo_set_timing(uint32_t rest_us, uint32_t active_us, uint32_t hold_ms);

#ifdef __cplusplus
}
#endif
