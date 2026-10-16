#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the internal captive portal HTTP web server on port 80.
 * @return ESP_OK on success.
 */
esp_err_t captive_portal_start(void);

/**
 * @brief Stop the captive portal HTTP web server.
 */
void captive_portal_stop(void);

#ifdef __cplusplus
}
#endif
