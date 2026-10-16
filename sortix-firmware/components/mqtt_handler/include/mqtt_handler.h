#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "sorting_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize MQTT client, configure topics and handlers, and begin connection.
 * @return ESP_OK on success.
 */
esp_err_t mqtt_handler_init(void);

/**
 * @brief Check if MQTT client is currently connected to broker.
 */
bool mqtt_handler_is_connected(void);

/**
 * @brief Publish periodic telemetry payload matching SortiX-Dashboard TelemetrySchema.
 */
esp_err_t mqtt_handler_publish_telemetry(void);

/**
 * @brief Publish heartbeat payload matching HeartbeatPayloadSchema.
 */
esp_err_t mqtt_handler_publish_heartbeat(void);

/**
 * @brief Publish status string ("IDLE", "RUNNING", "STOPPED", "JAMMED").
 * @param status_str String literal.
 */
esp_err_t mqtt_handler_publish_status(const char *status_str);

/**
 * @brief Publish completed item classification record matching ClassificationRecordSchema.
 * @param item Pointer to tracked_item_t.
 */
esp_err_t mqtt_handler_publish_classification(const tracked_item_t *item);

/**
 * @brief Publish mechanical jam event matching JamDetectedPayloadSchema.
 * @param duration_seconds Duration in seconds that conveyor was stalled.
 */
esp_err_t mqtt_handler_publish_jam(float duration_seconds);

/**
 * @brief Publish on-chip temperature warning matching TemperatureWarningPayloadSchema.
 * @param current_temp Current temperature in Celsius.
 * @param threshold_temp Trigger threshold in Celsius.
 */
esp_err_t mqtt_handler_publish_temp_warning(float current_temp, float threshold_temp);

/**
 * @brief Publish response status to SorterConfig set request.
 * @param applied Whether config was applied immediately or staged.
 * @param version Active or staged config version.
 * @param error_reason Rejection reason if not applied, or NULL.
 */
esp_err_t mqtt_handler_publish_config_status(bool applied, uint32_t version, const char *error_reason);

#ifdef __cplusplus
}
#endif
