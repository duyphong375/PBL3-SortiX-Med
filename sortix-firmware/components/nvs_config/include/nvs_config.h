#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char mqtt_broker_uri[128];
    char http_telemetry_url[128];
    char device_id[32];
    uint32_t active_config_version;
    int fallback_policy;        // 0 = PAUSE_AND_HOLD, 1 = DIVERT_TO_DEFAULT
    float counts_per_mm;
    float dist_s1_servo1_mm;
    float dist_s1_servo2_mm;
    float dist_s1_end_mm;
} sortix_sys_config_t;

/**
 * @brief Initialize NVS flash subsystem and load configuration.
 *        If no config exists in NVS, loads firmware default values.
 * @return ESP_OK on success.
 */
esp_err_t nvs_config_init(void);

/**
 * @brief Get pointer to global active system configuration.
 */
const sortix_sys_config_t *nvs_config_get(void);

/**
 * @brief Update and save system configuration to NVS.
 * @param cfg New configuration structure.
 * @return ESP_OK on success.
 */
esp_err_t nvs_config_save(const sortix_sys_config_t *cfg);

/**
 * @brief Reset configuration in NVS to factory defaults.
 */
esp_err_t nvs_config_factory_reset(void);

#ifdef __cplusplus
}
#endif
