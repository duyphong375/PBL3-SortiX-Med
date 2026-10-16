#include "captive_portal.h"
#include "nvs_config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "CAPTIVE_PORTAL";
static httpd_handle_t s_server = NULL;

static const char s_html_page[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>SortiX Config Portal</title>"
"<style>body{font-family:sans-serif;background:#0f172a;color:#f8fafc;padding:20px;max-width:480px;margin:auto;}"
"h2{color:#38bdf8;}label{display:block;margin-top:12px;font-size:14px;color:#94a3b8;}"
"input{width:100%;padding:10px;margin-top:4px;border:1px solid #334155;border-radius:6px;background:#1e293b;color:#fff;box-sizing:border-box;}"
"button{width:100%;padding:12px;margin-top:20px;background:#0284c7;color:#fff;border:none;border-radius:6px;font-size:16px;font-weight:bold;cursor:pointer;}"
"button:hover{background:#0369a1;}</style></head>"
"<body><h2>SortiX Device Setup</h2>"
"<form method='POST' action='/save'>"
"<label>WiFi SSID</label><input type='text' name='ssid' required>"
"<label>WiFi Password</label><input type='password' name='pass'>"
"<label>MQTT Broker URI</label><input type='text' name='mqtt' value='mqtt://192.168.1.100:1883'>"
"<label>HTTP Telemetry URL</label><input type='text' name='http' value='http://192.168.1.100:5000/api/telemetry'>"
"<button type='submit'>Save & Restart</button>"
"</form></body></html>";

static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, s_html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    sortix_sys_config_t new_cfg;
    memcpy(&new_cfg, nvs_config_get(), sizeof(sortix_sys_config_t));

    // Parse URL-encoded form data (ssid=..., pass=..., mqtt=..., http=...)
    char *token = strtok(buf, "&");
    while (token != NULL) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            char *key = token;
            char *val = eq + 1;
            if (strcmp(key, "ssid") == 0) {
                strncpy(new_cfg.wifi_ssid, val, sizeof(new_cfg.wifi_ssid) - 1);
            } else if (strcmp(key, "pass") == 0) {
                strncpy(new_cfg.wifi_pass, val, sizeof(new_cfg.wifi_pass) - 1);
            } else if (strcmp(key, "mqtt") == 0) {
                strncpy(new_cfg.mqtt_broker_uri, val, sizeof(new_cfg.mqtt_broker_uri) - 1);
            } else if (strcmp(key, "http") == 0) {
                strncpy(new_cfg.http_telemetry_url, val, sizeof(new_cfg.http_telemetry_url) - 1);
            }
        }
        token = strtok(NULL, "&");
    }

    nvs_config_save(&new_cfg);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, "<h3>Saved successfully! Restarting device...</h3>", HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
    return ESP_OK;
}

esp_err_t captive_portal_start(void)
{
    if (s_server != NULL) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 4;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    httpd_register_uri_handler(s_server, &root_uri);

    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post_handler,
    };
    httpd_register_uri_handler(s_server, &save_uri);

    ESP_LOGI(TAG, "Captive portal HTTP server started on port 80");
    return ESP_OK;
}

void captive_portal_stop(void)
{
    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "Captive portal HTTP server stopped");
    }
}
