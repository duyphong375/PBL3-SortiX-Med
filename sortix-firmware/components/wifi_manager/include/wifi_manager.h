#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_STATE_DISCONNECTED = 0,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_FALLBACK
} wifi_manager_state_t;

/**
 * @brief Initialize Wi-Fi station and connect using configured credentials.
 * @return ESP_OK on success.
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Check if Wi-Fi Station has obtained an IP address.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get current Wi-Fi RSSI (signal strength in dBm).
 * @return RSSI value (e.g., -58), or -100 if disconnected.
 */
int8_t wifi_manager_get_rssi(void);

/**
 * @brief Get current IP address as string (IPv4).
 * @param buffer Output char buffer (min 16 bytes).
 * @param max_len Size of buffer.
 */
void wifi_manager_get_ip_str(char *buffer, size_t max_len);

/**
 * @brief Get current Wi-Fi band string ("2.4 GHz" or "5.0 GHz (Wi-Fi 6)").
 */
const char *wifi_manager_get_band_str(void);

/**
 * @brief Force switch to SoftAP fallback mode for captive portal configuration.
 */
esp_err_t wifi_manager_start_softap(void);

#ifdef __cplusplus
}
#endif
