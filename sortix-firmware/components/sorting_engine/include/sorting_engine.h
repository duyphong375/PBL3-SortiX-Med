#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SORTING_MAX_TRACKED_ITEMS   15
#define SORTING_MAX_BRAND_IDS       16
#define SORTING_MAX_BRAND_LEN       32

typedef enum {
    ITEM_STATE_FREE = 0,
    ITEM_STATE_NEW,             // S1 triggered, waiting for vision result
    ITEM_STATE_IN_TRANSIT,      // Vision result paired, moving along belt
    ITEM_STATE_COMPLETED,       // Reached target bin or end of belt
    ITEM_STATE_TIMEOUT          // Vision timeout occurred
} tracked_item_state_t;

typedef struct {
    uint32_t id;
    char product_id[64];
    char brand_id[64];
    float confidence;
    int64_t entry_encoder;
    int64_t entry_timestamp_us;
    int target_bin;             // 1, 2, or 3
    int routed_bin;             // 0 = no route completed; 1, 2, or 3 = firmware route decision/actuation
    int actual_bin;             // 0 = physical destination unverified; 1, 2, or 3 only with feedback
    tracked_item_state_t state;
    char status[32];            // Includes "unverified" when actuator command has no destination feedback
    bool triggered;
    bool reported_error;        // Flag to prevent duplicate error callbacks
    bool servo_failed;          // Flag indicating servo actuation failed
} tracked_item_t;

// Structure for dynamic bin configuration
typedef struct {
    int bin_id;                 // 1 or 2
    int brand_count;
    char brand_ids[SORTING_MAX_BRAND_IDS][SORTING_MAX_BRAND_LEN];
} bin_rule_t;

typedef struct {
    uint32_t config_version;
    int bin_rule_count;
    bin_rule_t bins[2];         // Bin 1 and Bin 2
    int default_bin;            // 3
    bool pending_apply;
} dynamic_sorter_config_t;

typedef void (*classification_cb_t)(const tracked_item_t *item, void *user_ctx);

/**
 * @brief Initialize sorting engine with item tracking ring buffer and default routing rules.
 * @return ESP_OK on success.
 */
esp_err_t sorting_engine_init(void);

/**
 * @brief Enqueue a new physical item detected by S1 IR sensor.
 * @param encoder_count PCNT encoder count at time of detection.
 * @param timestamp_us System microsecond timestamp at detection.
 * @return ESP_OK on success, ESP_ERR_NO_MEM if queue is full.
 */
esp_err_t sorting_engine_on_s1_entry(int64_t encoder_count, int64_t timestamp_us);

/** Latch a tracking loss, stop the conveyor, and require operator clearance. */
void sorting_engine_note_tracking_overflow(void);

/**
 * @brief Process an incoming Vision AI YOLOv8 result from MQTT.
 *        Pairs FIFO with the oldest unassigned item.
 * @param product_id Product UUID string.
 * @param brand_id Brand classification identifier.
 * @param confidence AI detection confidence (0.0 to 1.0).
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no unassigned items in queue.
 */
esp_err_t sorting_engine_on_vision_result(const char *product_id, const char *brand_id, float confidence);

/**
 * @brief Periodic tracking update routine.
 *        Calculates item positions relative to encoder, triggers servos,
 *        handles timeouts, and dispatches classification records.
 */
void sorting_engine_update(void);

/**
 * @brief Apply or stage a new dynamic SorterConfig received from Dashboard.
 * @param json_str JSON payload of SorterConfigSchema.
 * @param out_applied Set to true if applied immediately, false if staged for line empty.
 * @param out_reason Buffer for rejection reason if error occurs.
 * @param reason_max_len Max length of reason buffer.
 * @return ESP_OK on success or valid staging, ESP_FAIL on JSON schema error.
 */
esp_err_t sorting_engine_apply_config_json(const char *json_str, bool *out_applied,
                                           char *out_reason, size_t reason_max_len);

/**
 * @brief Register callback for completed classification records.
 */
void sorting_engine_set_classification_callback(classification_cb_t cb, void *user_ctx);

/**
 * @brief Check if the conveyor belt is currently empty (0 items being tracked).
 */
bool sorting_engine_is_line_empty(void);

/**
 * @brief Check if there are unresolved held/timeout items on the conveyor belt.
 */
bool sorting_engine_has_unresolved_items(void);

/**
 * @brief Safely clear and free all unresolved timeout/held items after operator verification.
 * @return ESP_OK on success.
 */
esp_err_t sorting_engine_clear_unresolved(void);

/**
 * @brief Get currently active config version number.
 */
uint32_t sorting_engine_get_active_config_version(void);

#ifdef __cplusplus
}
#endif
