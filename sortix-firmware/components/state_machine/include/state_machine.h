#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYSTEM_STATE_IDLE = 0,      // Safe default boot state (Waiting for START)
    SYSTEM_STATE_RUNNING,       // Conveyor active and running
    SYSTEM_STATE_STOPPED,       // Controlled stop by operator or vision timeout
    SYSTEM_STATE_JAMMED         // Mechanical jam / encoder stall locked
} system_state_t;

typedef enum {
    SYSTEM_EVENT_START = 0,     // Start button pressed or MQTT START command
    SYSTEM_EVENT_STOP,          // MQTT STOP command or Pause-and-hold trigger
    SYSTEM_EVENT_JAM_DETECTED,  // Encoder stall detected while running
    SYSTEM_EVENT_ESTOP_RELEASE  // MQTT ESTOP_RELEASE (unlocks jam to IDLE, does NOT start motor)
} system_event_t;

typedef void (*state_change_cb_t)(system_state_t old_state, system_state_t new_state, void *user_ctx);

/**
 * @brief Initialize system state machine.
 *        Always initializes to safe SYSTEM_STATE_IDLE.
 * @return ESP_OK on success.
 */
esp_err_t state_machine_init(void);

/**
 * @brief Request an event transition in the state machine.
 *        Thread-safe.
 * @param event Requested event.
 * @return ESP_OK if transition accepted, ESP_ERR_INVALID_STATE if rejected.
 */
esp_err_t state_machine_process_event(system_event_t event);

/**
 * @brief Get current system state.
 */
system_state_t state_machine_get_state(void);

/**
 * @brief Get string representation of current state ("IDLE", "RUNNING", "STOPPED", "JAMMED").
 */
const char *state_machine_get_state_str(system_state_t state);

/**
 * @brief Register callback for state change notifications.
 */
void state_machine_register_callback(state_change_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif
