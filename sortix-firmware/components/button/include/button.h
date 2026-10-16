#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*button_cb_t)(void *user_ctx);

/**
 * @brief Initialize Start Button on GPIO23.
 *        Configures falling edge interrupt and 20ms debounce software timer.
 * @param on_pressed_cb Callback fired when a verified press occurs.
 * @param user_ctx User context pointer passed to callback.
 * @return ESP_OK on success.
 */
esp_err_t button_init(button_cb_t on_pressed_cb, void *user_ctx);

/**
 * @brief Get instantaneous physical level of the button pin.
 * @return 0 if pressed (LOW), 1 if released (HIGH).
 */
int button_get_level(void);

#ifdef __cplusplus
}
#endif
