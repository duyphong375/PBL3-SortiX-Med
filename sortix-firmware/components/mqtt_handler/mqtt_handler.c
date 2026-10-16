#include "mqtt_handler.h"
#include "app_config.h"
#include "nvs_config.h"
#include "wifi_manager.h"
#include "state_machine.h"
#include "sorting_engine.h"
#include "encoder.h"
#include "safety_monitor.h"
#include "sensors.h"
#include "servo.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <time.h>
#include <string.h>

static const char *TAG = "MQTT_HANDLER";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_is_connected = false;

static void get_iso8601_timestamp(char *buf, size_t max_len)
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);

    // If time is not synced via SNTP (pre-1971), format relative uptime
    if (timeinfo.tm_year < 71) {
        uint32_t uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
        snprintf(buf, max_len, "1970-01-01T%02lu:%02lu:%02luZ",
                 (unsigned long)((uptime_s / 3600) % 24),
                 (unsigned long)((uptime_s / 60) % 60),
                 (unsigned long)(uptime_s % 60));
    } else {
        strftime(buf, max_len, "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    }
}

static void handle_incoming_vision(const char *data, int data_len)
{
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse vision JSON payload");
        return;
    }

    cJSON *prod_id = cJSON_GetObjectItem(root, "product_id");
    cJSON *brand_id = cJSON_GetObjectItem(root, "brand_id");
    cJSON *conf = cJSON_GetObjectItem(root, "confidence");

    const char *p_str = cJSON_IsString(prod_id) ? prod_id->valuestring : "";
    const char *b_str = cJSON_IsString(brand_id) ? brand_id->valuestring : "";
    float confidence = cJSON_IsNumber(conf) ? (float)conf->valuedouble : 0.95f;

    ESP_LOGI(TAG, "Vision payload parsed: product='%s', brand='%s', conf=%.2f",
             p_str, b_str, confidence);
    sorting_engine_on_vision_result(p_str, b_str, confidence);

    cJSON_Delete(root);
}

static void handle_incoming_control(const char *data, int data_len)
{
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse control JSON payload");
        return;
    }

    cJSON *token = cJSON_GetObjectItem(root, "auth_token");
    if (token == NULL) {
        token = cJSON_GetObjectItem(root, "token");
    }
    const char *token_str = cJSON_IsString(token) ? token->valuestring : "";
    if (strcmp(token_str, SORTIX_CONTROL_SECRET) != 0) {
        ESP_LOGW(TAG, "SECURITY ALERT: Rejected unauthenticated control command (invalid or missing auth_token)!");
        cJSON_Delete(root);
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "command");
    if (cJSON_IsString(cmd)) {
        const char *cmd_str = cmd->valuestring;
        ESP_LOGI(TAG, "Authenticated control command received: '%s'", cmd_str);

        if (strcmp(cmd_str, "START") == 0) {
            state_machine_process_event(SYSTEM_EVENT_START);
        } else if (strcmp(cmd_str, "STOP") == 0) {
            state_machine_process_event(SYSTEM_EVENT_STOP);
        } else if (strcmp(cmd_str, "ESTOP_RELEASE") == 0) {
            state_machine_process_event(SYSTEM_EVENT_ESTOP_RELEASE);
        } else {
            ESP_LOGW(TAG, "Unknown control command: '%s'", cmd_str);
        }
    }
    cJSON_Delete(root);
}

static void handle_incoming_estop_release(const char *data, int data_len)
{
    bool authorized = false;
    if (data != NULL && data_len > 0) {
        cJSON *root = cJSON_ParseWithLength(data, data_len);
        if (root != NULL) {
            cJSON *token = cJSON_GetObjectItem(root, "auth_token");
            if (token == NULL) {
                token = cJSON_GetObjectItem(root, "token");
            }
            if (cJSON_IsString(token) && strcmp(token->valuestring, SORTIX_CONTROL_SECRET) == 0) {
                authorized = true;
            }
            cJSON_Delete(root);
        }
    }

    if (!authorized) {
        ESP_LOGW(TAG, "SECURITY ALERT: Rejected unauthenticated ESTOP_RELEASE (invalid or missing auth_token)!");
        return;
    }

    ESP_LOGI(TAG, "Authenticated safety ESTOP_RELEASE topic event accepted");
    state_machine_process_event(SYSTEM_EVENT_ESTOP_RELEASE);
}

static void handle_incoming_config_set(const char *data, int data_len)
{
    cJSON *root = cJSON_ParseWithLength(data, data_len);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse config JSON payload");
        return;
    }

    cJSON *token = cJSON_GetObjectItem(root, "auth_token");
    if (token == NULL) {
        token = cJSON_GetObjectItem(root, "token");
    }
    const char *token_str = cJSON_IsString(token) ? token->valuestring : "";
    if (strcmp(token_str, SORTIX_CONTROL_SECRET) != 0) {
        ESP_LOGW(TAG, "SECURITY ALERT: Rejected unauthenticated config/set (invalid or missing auth_token)!");
        uint32_t current_ver = sorting_engine_get_active_config_version();
        mqtt_handler_publish_config_status(false, current_ver, "UNAUTHORIZED: Invalid or missing auth_token");
        cJSON_Delete(root);
        return;
    }
    cJSON_Delete(root);

    char *payload_str = malloc(data_len + 1);
    if (payload_str == NULL) return;
    memcpy(payload_str, data, data_len);
    payload_str[data_len] = '\0';

    bool applied = false;
    char reason[128] = {0};
    esp_err_t ret = sorting_engine_apply_config_json(payload_str, &applied, reason, sizeof(reason));
    free(payload_str);

    uint32_t version = sorting_engine_get_active_config_version();
    if (ret == ESP_OK) {
        mqtt_handler_publish_config_status(applied, version, NULL);
    } else {
        mqtt_handler_publish_config_status(false, version, reason);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            s_is_connected = true;
            ESP_LOGI(TAG, "MQTT Connected to broker! Subscribing to SortiX topics...");

            esp_mqtt_client_subscribe(s_mqtt_client, SORTIX_TOPIC_VISION, 1);
            esp_mqtt_client_subscribe(s_mqtt_client, SORTIX_TOPIC_CONTROL, 1);
            esp_mqtt_client_subscribe(s_mqtt_client, SORTIX_TOPIC_ESTOP_RELEASE, 1);
            esp_mqtt_client_subscribe(s_mqtt_client, SORTIX_TOPIC_CONFIG_SET, 1);

            // Publish initial status
            mqtt_handler_publish_status(state_machine_get_state_str(state_machine_get_state()));
            break;

        case MQTT_EVENT_DISCONNECTED:
            s_is_connected = false;
            ESP_LOGW(TAG, "MQTT Disconnected from broker");
            break;

        case MQTT_EVENT_DATA: {
            char topic[128];
            int topic_len = event->topic_len < (sizeof(topic) - 1) ? event->topic_len : (sizeof(topic) - 1);
            memcpy(topic, event->topic, topic_len);
            topic[topic_len] = '\0';

            if (strcmp(topic, SORTIX_TOPIC_VISION) == 0) {
                handle_incoming_vision(event->data, event->data_len);
            } else if (strcmp(topic, SORTIX_TOPIC_CONTROL) == 0) {
                handle_incoming_control(event->data, event->data_len);
            } else if (strcmp(topic, SORTIX_TOPIC_ESTOP_RELEASE) == 0) {
                handle_incoming_estop_release(event->data, event->data_len);
            } else if (strcmp(topic, SORTIX_TOPIC_CONFIG_SET) == 0) {
                handle_incoming_config_set(event->data, event->data_len);
            }
            break;
        }

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT Error event occurred");
            break;

        default:
            break;
    }
}

esp_err_t mqtt_handler_init(void)
{
    const sortix_sys_config_t *sys_cfg = nvs_config_get();
    ESP_LOGI(TAG, "Initializing MQTT Client -> Broker: %s", sys_cfg->mqtt_broker_uri);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address.uri = sys_cfg->mqtt_broker_uri,
        },
        .network = {
            .reconnect_timeout_ms = 3000,
        },
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client handle");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(s_mqtt_client);
}

bool mqtt_handler_is_connected(void)
{
    return s_is_connected;
}

esp_err_t mqtt_handler_publish_telemetry(void)
{
    if (!s_is_connected || s_mqtt_client == NULL) return ESP_ERR_INVALID_STATE;

    system_state_t state = state_machine_get_state();
    bool is_running = (state == SYSTEM_STATE_RUNNING);
    int speed_val = is_running ? CONVEYOR_NOMINAL_SPEED_PCT : 0; // 0-100% scale

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", SORTIX_DEVICE_ID);
    cJSON_AddBoolToObject(root, "online", true);
    cJSON_AddNumberToObject(root, "uptime", (double)(esp_timer_get_time() / 1000000));
    cJSON_AddNumberToObject(root, "cpu_temp", (double)safety_monitor_get_cpu_temp());
    cJSON_AddNumberToObject(root, "wifi_rssi", (double)wifi_manager_get_rssi());
    cJSON_AddStringToObject(root, "wifi_band", wifi_manager_get_band_str());
    cJSON_AddBoolToObject(root, "conveyor_running", is_running);
    cJSON_AddNumberToObject(root, "conveyor_speed", speed_val);
    cJSON_AddBoolToObject(root, "s1_entry", sensors_is_s1_blocked());
    cJSON_AddBoolToObject(root, "s2_sorter1", false);
    cJSON_AddBoolToObject(root, "s3_sorter2", false);
    cJSON_AddBoolToObject(root, "arm1_active", servo_is_busy(SERVO_ID_1));
    cJSON_AddBoolToObject(root, "arm2_active", servo_is_busy(SERVO_ID_2));
    cJSON_AddNumberToObject(root, "encoder_count", (double)encoder_get_count());
    cJSON_AddNumberToObject(root, "active_config_version", (double)sorting_engine_get_active_config_version());

    char timestamp_buf[32];
    get_iso8601_timestamp(timestamp_buf, sizeof(timestamp_buf));
    cJSON_AddStringToObject(root, "last_heartbeat", timestamp_buf);

    char *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_out != NULL) {
        esp_mqtt_client_publish(s_mqtt_client, SORTIX_TOPIC_TELEMETRY, json_out, 0, 1, 0);
        free(json_out);
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t mqtt_handler_publish_heartbeat(void)
{
    if (!s_is_connected || s_mqtt_client == NULL) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", SORTIX_DEVICE_ID);
    cJSON_AddNumberToObject(root, "uptime", (double)(esp_timer_get_time() / 1000000));

    char timestamp_buf[32];
    get_iso8601_timestamp(timestamp_buf, sizeof(timestamp_buf));
    cJSON_AddStringToObject(root, "timestamp", timestamp_buf);

    char *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_out != NULL) {
        esp_mqtt_client_publish(s_mqtt_client, SORTIX_TOPIC_HEARTBEAT, json_out, 0, 0, 0);
        free(json_out);
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t mqtt_handler_publish_status(const char *status_str)
{
    if (!s_is_connected || s_mqtt_client == NULL || status_str == NULL) return ESP_ERR_INVALID_STATE;
    esp_mqtt_client_publish(s_mqtt_client, SORTIX_TOPIC_STATUS, status_str, 0, 1, 1);
    return ESP_OK;
}

esp_err_t mqtt_handler_publish_classification(const tracked_item_t *item)
{
    if (!s_is_connected || s_mqtt_client == NULL || item == NULL) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    char uuid_buf[64];
    snprintf(uuid_buf, sizeof(uuid_buf), "sort_%lu_%lld", item->id, esp_timer_get_time() / 1000);

    cJSON_AddStringToObject(root, "id", uuid_buf);
    cJSON_AddStringToObject(root, "product_id", item->product_id);
    cJSON_AddStringToObject(root, "brand_id", item->brand_id);
    cJSON_AddStringToObject(root, "brand_name", item->brand_id);
    cJSON_AddNumberToObject(root, "confidence", (double)item->confidence);
    cJSON_AddNumberToObject(root, "target_bin", item->target_bin);
    cJSON_AddNumberToObject(root, "actual_bin", item->actual_bin);
    cJSON_AddStringToObject(root, "status", item->status);

    char timestamp_buf[32];
    get_iso8601_timestamp(timestamp_buf, sizeof(timestamp_buf));
    cJSON_AddStringToObject(root, "timestamp", timestamp_buf);

    char *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_out != NULL) {
        esp_mqtt_client_publish(s_mqtt_client, SORTIX_TOPIC_CLASSIFICATION, json_out, 0, 1, 0);
        free(json_out);
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t mqtt_handler_publish_jam(float duration_seconds)
{
    if (!s_is_connected || s_mqtt_client == NULL) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "event", "jam_detected");
    cJSON_AddStringToObject(root, "section", "Conveyor_Belt_Zone_A");
    cJSON_AddNumberToObject(root, "duration_seconds", (double)duration_seconds);
    cJSON_AddStringToObject(root, "sensor_id", "ENCODER_STALL_01");

    char timestamp_buf[32];
    get_iso8601_timestamp(timestamp_buf, sizeof(timestamp_buf));
    cJSON_AddStringToObject(root, "timestamp", timestamp_buf);

    char *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_out != NULL) {
        esp_mqtt_client_publish(s_mqtt_client, SORTIX_TOPIC_JAM, json_out, 0, 1, 0);
        free(json_out);
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t mqtt_handler_publish_temp_warning(float current_temp, float threshold_temp)
{
    if (!s_is_connected || s_mqtt_client == NULL) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "event", "temperature_warning");
    cJSON_AddStringToObject(root, "device_name", "ESP32-C5 SoC");
    cJSON_AddNumberToObject(root, "current_temp", (double)current_temp);
    cJSON_AddNumberToObject(root, "threshold_temp", (double)threshold_temp);
    cJSON_AddStringToObject(root, "unit", "°C");

    char timestamp_buf[32];
    get_iso8601_timestamp(timestamp_buf, sizeof(timestamp_buf));
    cJSON_AddStringToObject(root, "timestamp", timestamp_buf);

    char *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_out != NULL) {
        esp_mqtt_client_publish(s_mqtt_client, SORTIX_TOPIC_TEMP_WARN, json_out, 0, 1, 0);
        free(json_out);
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t mqtt_handler_publish_config_status(bool applied, uint32_t version, const char *error_reason)
{
    if (!s_is_connected || s_mqtt_client == NULL) return ESP_ERR_INVALID_STATE;

    cJSON *root = cJSON_CreateObject();
    if (error_reason == NULL) {
        cJSON_AddStringToObject(root, "status", applied ? "applied" : "staged");
        cJSON_AddNumberToObject(root, "config_version", version);
    } else {
        cJSON_AddStringToObject(root, "status", "rejected");
        cJSON_AddStringToObject(root, "reason", error_reason);
    }

    char *json_out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_out != NULL) {
        esp_mqtt_client_publish(s_mqtt_client, SORTIX_TOPIC_CONFIG_STATUS, json_out, 0, 1, 0);
        free(json_out);
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}
