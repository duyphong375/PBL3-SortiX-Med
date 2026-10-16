#include "http_telemetry.h"
#include "app_config.h"
#include "nvs_config.h"
#include "state_machine.h"
#include "safety_monitor.h"
#include "encoder.h"
#include "sensors.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "HTTP_TELEMETRY";

esp_err_t http_telemetry_init(void)
{
    ESP_LOGI(TAG, "HTTP Telemetry initialized (Endpoint: %s)", nvs_config_get()->http_telemetry_url);
    return ESP_OK;
}

esp_err_t http_telemetry_send_post(void)
{
    const sortix_sys_config_t *sys_cfg = nvs_config_get();
    if (strlen(sys_cfg->http_telemetry_url) == 0) return ESP_ERR_INVALID_STATE;

    system_state_t state = state_machine_get_state();
    bool is_running = (state == SYSTEM_STATE_RUNNING);
    int speed_val = is_running ? CONVEYOR_NOMINAL_SPEED_PCT : 0; // 0-100%

    // Construct telemetry JSON payload
    // STRICT: Do NOT include "optical_sensor" field to avoid conflicting auto-stop logic in backend!
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", SORTIX_DEVICE_ID);
    cJSON_AddNumberToObject(root, "cpu_temp", (double)safety_monitor_get_cpu_temp());
    cJSON_AddNumberToObject(root, "conveyor_speed", speed_val);
    cJSON_AddBoolToObject(root, "conveyor_running", is_running);
    cJSON_AddBoolToObject(root, "s1_entry", sensors_is_s1_blocked());
    cJSON_AddNumberToObject(root, "encoder_count", (double)encoder_get_count());

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (post_data == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t config = {
        .url = sys_cfg->http_telemetry_url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 4000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        free(post_data);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGD(TAG, "HTTP POST Telemetry sent (Status %d)", status_code);
    } else {
        ESP_LOGW(TAG, "HTTP POST Telemetry failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(post_data);
    return err;
}
