#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MOTOR_DIR_COAST = 0,    // IN1=0, IN2=0
    MOTOR_DIR_FORWARD,      // IN1=1, IN2=0
    MOTOR_DIR_REVERSE,      // IN1=0, IN2=1
    MOTOR_DIR_BRAKE         // IN1=1, IN2=1
} motor_direction_t;

/**
 * @brief Initialize motor control GPIOs (IN1: GPIO8, IN2: GPIO9).
 *        Motor starts in COAST / STOPPED state.
 * @return ESP_OK on success.
 */
esp_err_t motor_control_init(void);

/**
 * @brief Drive conveyor forward (standard sorting operation).
 * @return ESP_OK on success.
 */
esp_err_t motor_control_forward(void);

/**
 * @brief Drive conveyor in reverse (e.g., jam clearing or calibration).
 * @return ESP_OK on success.
 */
esp_err_t motor_control_reverse(void);

/**
 * @brief Apply dynamic braking to conveyor motor (IN1=1, IN2=1).
 * @return ESP_OK on success.
 */
esp_err_t motor_control_brake(void);

/**
 * @brief Turn off power to motor / coast to stop (IN1=0, IN2=0).
 * @return ESP_OK on success.
 */
esp_err_t motor_control_stop(void);

/**
 * @brief Check if the motor is currently running forward.
 * @return true if running forward, false otherwise.
 */
bool motor_control_is_running(void);

/**
 * @brief Get current motor direction state.
 */
motor_direction_t motor_control_get_direction(void);

#ifdef __cplusplus
}
#endif
