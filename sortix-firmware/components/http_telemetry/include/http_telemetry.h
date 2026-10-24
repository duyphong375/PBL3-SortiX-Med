#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize HTTP Telemetry client.
 * @return ESP_OK on success.
 */
esp_err_t http_telemetry_init(void);

/**
 * @brief Send one HTTP POST telemetry payload to the backend server (port 5000).
 *        Payload includes device_id, valid CPU temperature, commanded motor state,
 *        encoder speed when fresh, S1 state, encoder count, and unknown S2/S3 state.
 *        DOES NOT include "optical_sensor".
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t http_telemetry_send_post(void);

#ifdef __cplusplus
}
#endif
