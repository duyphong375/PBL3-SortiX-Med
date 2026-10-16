#include "sorting_engine.h"
#include "encoder.h"
#include "servo.h"
#include "state_machine.h"
#include "nvs_config.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "SORTING_ENGINE";

#define DIST_S1_TO_SERVO1_MM        200.0f
#define DIST_S1_TO_SERVO2_MM        400.0f
#define DIST_S1_TO_END_MM           540.0f
#define SERVO_TRIGGER_WINDOW_MM     15.0f
#define VISION_TIMEOUT_US           1000000 // 1.0s

static tracked_item_t s_items[SORTING_MAX_TRACKED_ITEMS];
static dynamic_sorter_config_t s_active_config;
static dynamic_sorter_config_t s_pending_config;
static SemaphoreHandle_t s_sorting_mutex = NULL;
static uint32_t s_item_seq_counter = 0;

static classification_cb_t s_classification_cb = NULL;
static void *s_classification_user_ctx = NULL;

static int lookup_target_bin(const dynamic_sorter_config_t *cfg, const char *brand_id)
{
    if (brand_id == NULL || strlen(brand_id) == 0) {
        return cfg->default_bin;
    }

    for (int i = 0; i < cfg->bin_rule_count; i++) {
        const bin_rule_t *rule = &cfg->bins[i];
        for (int j = 0; j < rule->brand_count; j++) {
            if (strcasecmp(rule->brand_ids[j], brand_id) == 0) {
                return rule->bin_id;
            }
        }
    }
    return cfg->default_bin;
}

esp_err_t sorting_engine_init(void)
{
    ESP_LOGI(TAG, "Initializing Sorting Engine");

    if (s_sorting_mutex == NULL) {
        s_sorting_mutex = xSemaphoreCreateMutex();
        if (s_sorting_mutex == NULL) return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);
    memset(s_items, 0, sizeof(s_items));
    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        s_items[i].state = ITEM_STATE_FREE;
    }

    // Default configuration:
    // Bin 1: Sharps Waste ("brand_b", "brand_c")
    // Bin 2: Surgical Instruments ("brand_a")
    // Bin 3: General Medical Supplies (Default)
    s_active_config.config_version = 1;
    s_active_config.bin_rule_count = 2;
    s_active_config.default_bin = 3;
    s_active_config.pending_apply = false;

    s_active_config.bins[0].bin_id = 1;
    s_active_config.bins[0].brand_count = 2;
    strncpy(s_active_config.bins[0].brand_ids[0], "brand_b", SORTING_MAX_BRAND_LEN);
    strncpy(s_active_config.bins[0].brand_ids[1], "brand_c", SORTING_MAX_BRAND_LEN);

    s_active_config.bins[1].bin_id = 2;
    s_active_config.bins[1].brand_count = 1;
    strncpy(s_active_config.bins[1].brand_ids[0], "brand_a", SORTING_MAX_BRAND_LEN);

    s_pending_config.pending_apply = false;
    xSemaphoreGive(s_sorting_mutex);

    ESP_LOGI(TAG, "Sorting engine initialized with active config version: %lu",
             s_active_config.config_version);
    return ESP_OK;
}

esp_err_t sorting_engine_on_s1_entry(int64_t encoder_count, int64_t timestamp_us)
{
    if (s_sorting_mutex == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = ESP_ERR_NO_MEM;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);

    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        if (s_items[i].state == ITEM_STATE_FREE) {
            tracked_item_t *item = &s_items[i];
            memset(item, 0, sizeof(tracked_item_t));
            item->id = ++s_item_seq_counter;
            snprintf(item->product_id, sizeof(item->product_id), "prod_%lu", item->id);
            item->entry_encoder = encoder_count;
            item->entry_timestamp_us = timestamp_us;
            item->state = ITEM_STATE_NEW;
            item->target_bin = s_active_config.default_bin;
            item->actual_bin = s_active_config.default_bin;
            item->confidence = 0.0f;
            item->triggered = false;
            strncpy(item->status, "success", sizeof(item->status));

            ESP_LOGI(TAG, "New item #%lu registered at S1 (Encoder: %lld, Time: %lld us)",
                     item->id, encoder_count, timestamp_us);
            ret = ESP_OK;
            break;
        }
    }

    xSemaphoreGive(s_sorting_mutex);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Tracking queue full! Dropped item at S1 entry.");
    }
    return ret;
}

esp_err_t sorting_engine_on_vision_result(const char *product_id, const char *brand_id, float confidence)
{
    if (s_sorting_mutex == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);

    // Find the oldest item in ITEM_STATE_NEW (FIFO order)
    tracked_item_t *oldest_new_item = NULL;
    int64_t oldest_time = INT64_MAX;

    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        if (s_items[i].state == ITEM_STATE_NEW) {
            if (s_items[i].entry_timestamp_us < oldest_time) {
                oldest_time = s_items[i].entry_timestamp_us;
                oldest_new_item = &s_items[i];
            }
        }
    }

    if (oldest_new_item != NULL) {
        if (product_id && strlen(product_id) > 0) {
            strncpy(oldest_new_item->product_id, product_id, sizeof(oldest_new_item->product_id) - 1);
        }
        if (brand_id && strlen(brand_id) > 0) {
            strncpy(oldest_new_item->brand_id, brand_id, sizeof(oldest_new_item->brand_id) - 1);
        }
        oldest_new_item->confidence = confidence;
        oldest_new_item->target_bin = lookup_target_bin(&s_active_config, brand_id);
        oldest_new_item->state = ITEM_STATE_IN_TRANSIT;

        ESP_LOGI(TAG, "Item #%lu paired with Vision: product='%s', brand='%s', conf=%.2f -> Target Bin %d",
                 oldest_new_item->id, oldest_new_item->product_id, oldest_new_item->brand_id,
                 confidence, oldest_new_item->target_bin);
        ret = ESP_OK;
    } else {
        ESP_LOGW(TAG, "Vision result received but no pending ITEM_STATE_NEW in queue!");
    }

    xSemaphoreGive(s_sorting_mutex);
    return ret;
}

void sorting_engine_update(void)
{
    if (s_sorting_mutex == NULL) return;

    int64_t current_encoder = encoder_get_count();
    int64_t now_us = esp_timer_get_time();
    const sortix_sys_config_t *sys_cfg = nvs_config_get();
    int fallback_policy = sys_cfg->fallback_policy;

    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);

    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        tracked_item_t *item = &s_items[i];

        // 1. Check Vision Timeout on items waiting in ITEM_STATE_NEW
        if (item->state == ITEM_STATE_NEW) {
            if ((now_us - item->entry_timestamp_us) >= VISION_TIMEOUT_US) {
                if (fallback_policy == 0) { // PAUSE_AND_HOLD (Safe default)
                    ESP_LOGE(TAG, "Item #%lu Vision TIMEOUT (%.1fs). Executing PAUSE_AND_HOLD!",
                             item->id, (float)VISION_TIMEOUT_US / 1000000.0f);
                    state_machine_process_event(SYSTEM_EVENT_STOP);
                    item->state = ITEM_STATE_TIMEOUT;
                    strncpy(item->status, "rejected", sizeof(item->status));
                } else { // DIVERT_TO_DEFAULT
                    ESP_LOGW(TAG, "Item #%lu Vision TIMEOUT. Diverting to default Bin 3.", item->id);
                    item->target_bin = s_active_config.default_bin;
                    item->actual_bin = s_active_config.default_bin;
                    strncpy(item->status, "diverted_default", sizeof(item->status));
                    item->state = ITEM_STATE_IN_TRANSIT;
                }
            }
        }

        // 2. Track items in transit along the conveyor
        if (item->state == ITEM_STATE_IN_TRANSIT) {
            float dist_mm = encoder_counts_to_mm(current_encoder - item->entry_encoder);

            if (item->target_bin == 1) {
                if (dist_mm >= (DIST_S1_TO_SERVO1_MM - SERVO_TRIGGER_WINDOW_MM) && !item->triggered) {
                    servo_trigger(SERVO_ID_1);
                    item->triggered = true;
                    item->actual_bin = 1;
                    item->state = ITEM_STATE_COMPLETED;
                    ESP_LOGI(TAG, "Item #%lu reached Servo 1 station (dist=%.1f mm). Diverting to Bin 1.",
                             item->id, dist_mm);
                }
            } else if (item->target_bin == 2) {
                if (dist_mm >= (DIST_S1_TO_SERVO2_MM - SERVO_TRIGGER_WINDOW_MM) && !item->triggered) {
                    servo_trigger(SERVO_ID_2);
                    item->triggered = true;
                    item->actual_bin = 2;
                    item->state = ITEM_STATE_COMPLETED;
                    ESP_LOGI(TAG, "Item #%lu reached Servo 2 station (dist=%.1f mm). Diverting to Bin 2.",
                             item->id, dist_mm);
                }
            } else {
                // Target is Bin 3 (Default / End of line)
                if (dist_mm >= DIST_S1_TO_END_MM) {
                    item->actual_bin = 3;
                    item->state = ITEM_STATE_COMPLETED;
                    ESP_LOGI(TAG, "Item #%lu reached end of line (dist=%.1f mm). Deposited in Bin 3.",
                             item->id, dist_mm);
                }
            }
        }

        // 3. Process completed items
        if (item->state == ITEM_STATE_COMPLETED) {
            if (s_classification_cb != NULL) {
                s_classification_cb(item, s_classification_user_ctx);
            }
            // Free the slot
            item->state = ITEM_STATE_FREE;
        }
    }

    // 4. Check if pending config can be applied when line is empty
    if (s_pending_config.pending_apply) {
        bool line_empty = true;
        for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
            if (s_items[i].state != ITEM_STATE_FREE) {
                line_empty = false;
                break;
            }
        }
        if (line_empty) {
            s_active_config = s_pending_config;
            s_active_config.pending_apply = false;
            s_pending_config.pending_apply = false;
            ESP_LOGI(TAG, "Pending SorterConfig version %lu applied (Line is empty)",
                     s_active_config.config_version);
        }
    }

    xSemaphoreGive(s_sorting_mutex);
}

esp_err_t sorting_engine_apply_config_json(const char *json_str, bool *out_applied,
                                           char *out_reason, size_t reason_max_len)
{
    if (json_str == NULL || out_applied == NULL) return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        if (out_reason) strncpy(out_reason, "Invalid JSON format", reason_max_len);
        return ESP_FAIL;
    }

    cJSON *cfg_ver = cJSON_GetObjectItem(root, "config_version");
    cJSON *bins_arr = cJSON_GetObjectItem(root, "bins");
    cJSON *apply_mode = cJSON_GetObjectItem(root, "apply_mode");

    if (!cJSON_IsNumber(cfg_ver) || !cJSON_IsArray(bins_arr)) {
        if (out_reason) strncpy(out_reason, "Missing config_version or bins array", reason_max_len);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    dynamic_sorter_config_t new_cfg;
    memset(&new_cfg, 0, sizeof(dynamic_sorter_config_t));
    new_cfg.config_version = cfg_ver->valueint;
    new_cfg.default_bin = 3;
    new_cfg.bin_rule_count = 0;

    int arr_size = cJSON_GetArraySize(bins_arr);
    if (arr_size > 2) arr_size = 2; // Only support Bin 1 and Bin 2 arms

    for (int i = 0; i < arr_size; i++) {
        cJSON *bin_obj = cJSON_GetArrayItem(bins_arr, i);
        cJSON *bin_id = cJSON_GetObjectItem(bin_obj, "bin_id");
        cJSON *brands = cJSON_GetObjectItem(bin_obj, "brand_ids");

        if (cJSON_IsNumber(bin_id) && cJSON_IsArray(brands)) {
            bin_rule_t *rule = &new_cfg.bins[new_cfg.bin_rule_count++];
            rule->bin_id = bin_id->valueint;
            rule->brand_count = 0;

            int b_size = cJSON_GetArraySize(brands);
            for (int j = 0; j < b_size && j < SORTING_MAX_BRAND_IDS; j++) {
                cJSON *b_item = cJSON_GetArrayItem(brands, j);
                if (cJSON_IsString(b_item)) {
                    strncpy(rule->brand_ids[rule->brand_count++],
                            b_item->valuestring, SORTING_MAX_BRAND_LEN - 1);
                }
            }
        }
    }

    bool immediate = false;
    if (cJSON_IsString(apply_mode) && strcmp(apply_mode->valuestring, "immediate") == 0) {
        immediate = true;
    }

    cJSON_Delete(root);

    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);
    bool line_empty = true;
    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        if (s_items[i].state != ITEM_STATE_FREE) {
            line_empty = false;
            break;
        }
    }

    if (immediate || line_empty) {
        s_active_config = new_cfg;
        s_active_config.pending_apply = false;
        *out_applied = true;
        ESP_LOGI(TAG, "SorterConfig version %lu applied immediately", new_cfg.config_version);
    } else {
        s_pending_config = new_cfg;
        s_pending_config.pending_apply = true;
        *out_applied = false;
        ESP_LOGI(TAG, "SorterConfig version %lu staged (waiting for line to empty)", new_cfg.config_version);
    }
    xSemaphoreGive(s_sorting_mutex);

    return ESP_OK;
}

void sorting_engine_set_classification_callback(classification_cb_t cb, void *user_ctx)
{
    s_classification_cb = cb;
    s_classification_user_ctx = user_ctx;
}

bool sorting_engine_is_line_empty(void)
{
    if (s_sorting_mutex == NULL) return true;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);
    bool empty = true;
    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        if (s_items[i].state != ITEM_STATE_FREE) {
            empty = false;
            break;
        }
    }
    xSemaphoreGive(s_sorting_mutex);
    return empty;
}

uint32_t sorting_engine_get_active_config_version(void)
{
    return s_active_config.config_version;
}
