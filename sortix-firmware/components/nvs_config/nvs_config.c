#include "nvs_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "NVS_CONFIG";
#define NVS_NAMESPACE   "sortix_cfg"
#define NVS_BLOB_KEY    "sys_cfg"

static sortix_sys_config_t s_config = {
    .wifi_ssid = "SortiX-Lab",
    .wifi_pass = "sortix1234",
    .mqtt_broker_uri = "mqtt://192.168.1.100:1883",
    .http_telemetry_url = "http://192.168.1.100:5000/api/telemetry",
    .device_id = "sorter_01",
    .active_config_version = 1,
    .fallback_policy = 0,               // PAUSE_AND_HOLD (Safe default)
    .counts_per_mm = 31.5f,
    .dist_s1_servo1_mm = 200.0f,
    .dist_s1_servo2_mm = 400.0f,
    .dist_s1_end_mm = 540.0f
};

esp_err_t nvs_config_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        return ret;
    }

    nvs_handle_t handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_OK) {
        size_t required_size = sizeof(sortix_sys_config_t);
        sortix_sys_config_t loaded;
        ret = nvs_get_blob(handle, NVS_BLOB_KEY, &loaded, &required_size);
        if (ret == ESP_OK && required_size == sizeof(sortix_sys_config_t)) {
            memcpy(&s_config, &loaded, sizeof(sortix_sys_config_t));
            ESP_LOGI(TAG, "Loaded configuration from NVS (SSID: %s, MQTT: %s)",
                     s_config.wifi_ssid, s_config.mqtt_broker_uri);
        } else {
            ESP_LOGW(TAG, "Config blob not found or size mismatch, using defaults");
        }
        nvs_close(handle);
    } else {
        ESP_LOGI(TAG, "No NVS namespace '%s', using default config", NVS_NAMESPACE);
    }

    return ESP_OK;
}

const sortix_sys_config_t *nvs_config_get(void)
{
    return &s_config;
}

esp_err_t nvs_config_save(const sortix_sys_config_t *cfg)
{
    if (cfg == NULL) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_blob(handle, NVS_BLOB_KEY, cfg, sizeof(sortix_sys_config_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
        if (ret == ESP_OK) {
            memcpy(&s_config, cfg, sizeof(sortix_sys_config_t));
            ESP_LOGI(TAG, "Configuration committed to NVS");
        }
    }
    nvs_close(handle);
    return ret;
}

esp_err_t nvs_config_factory_reset(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
        ESP_LOGW(TAG, "NVS namespace erased to factory defaults");
    }
    return ret;
}
