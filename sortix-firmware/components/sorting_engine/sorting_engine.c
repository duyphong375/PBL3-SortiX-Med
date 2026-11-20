#include "sorting_engine.h"
#include "app_config.h"
#include "encoder.h"
#include "motor_control.h"
#include "servo.h"
#include "state_machine.h"
#include "nvs_config.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char *TAG = "SORTING_ENGINE";

#define CAMERA_SCAN_PAUSE_US        ((int64_t)CAMERA_SCAN_PAUSE_MS * 1000LL) // Dừng 2.0s tại buồng quét DroidCam (10cm)
#define BIN3_DROP_HOLD_US           500000LL                                 // Dừng 0.5s xác nhận rơi vào Ô chứa 3 (65cm)

static tracked_item_t s_items[SORTING_MAX_TRACKED_ITEMS];
static dynamic_sorter_config_t s_active_config;
static dynamic_sorter_config_t s_pending_config;
static SemaphoreHandle_t s_sorting_mutex = NULL;
static uint32_t s_item_seq_counter = 0;
static bool s_tracking_overflow_latched = false;
static bool s_loading_persisted_config = false;
static int64_t s_last_encoder_sample = 0;
static bool s_encoder_sample_valid = false;
static int64_t s_last_entry_encoder = 0;
static int64_t s_last_entry_timestamp_us = 0;
static bool s_has_previous_entry = false;

static classification_cb_t s_classification_cb = NULL;
static void *s_classification_user_ctx = NULL;

static void set_reason(char *buffer, size_t buffer_size, const char *format, ...)
{
    if (buffer == NULL || buffer_size == 0 || format == NULL) return;
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, buffer_size, format, args);
    va_end(args);
}

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
    memset(&s_active_config, 0, sizeof(s_active_config));
    memset(&s_pending_config, 0, sizeof(s_pending_config));
    s_tracking_overflow_latched = false;
    s_last_encoder_sample = encoder_get_count();
    s_encoder_sample_valid = true;
    s_has_previous_entry = false;
    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        s_items[i].state = ITEM_STATE_FREE;
    }

    // Cấu hình phân loại mặc định (hỗ trợ cả med_* mới lẫn brand_* tương thích ngược):
    // Khay 1: Vật sắc nhọn ("med_syringe", "brand_b", "brand_c")
    // Khay 2: Dụng cụ phẫu thuật ("med_scissors", "med_forceps", "brand_a")
    // Khay 3: Vật tư y tế / Mặc định ("med_vial" / chưa nhận diện)
    s_active_config.config_version = 1;
    s_active_config.bin_rule_count = 2;
    s_active_config.default_bin = 3;
    s_active_config.pending_apply = false;

    s_active_config.bins[0].bin_id = 1;
    s_active_config.bins[0].brand_count = 3;
    strncpy(s_active_config.bins[0].brand_ids[0], "med_syringe", SORTING_MAX_BRAND_LEN);
    strncpy(s_active_config.bins[0].brand_ids[1], "brand_b", SORTING_MAX_BRAND_LEN);
    strncpy(s_active_config.bins[0].brand_ids[2], "brand_c", SORTING_MAX_BRAND_LEN);

    s_active_config.bins[1].bin_id = 2;
    s_active_config.bins[1].brand_count = 3;
    strncpy(s_active_config.bins[1].brand_ids[0], "med_scissors", SORTING_MAX_BRAND_LEN);
    strncpy(s_active_config.bins[1].brand_ids[1], "med_forceps", SORTING_MAX_BRAND_LEN);
    strncpy(s_active_config.bins[1].brand_ids[2], "brand_a", SORTING_MAX_BRAND_LEN);

    s_pending_config.pending_apply = false;
    xSemaphoreGive(s_sorting_mutex);

    ESP_LOGI(TAG, "Sorting engine initialized with active config version: %lu",
             s_active_config.config_version);

    char *saved_config = malloc(4096);
    if (saved_config == NULL) {
        ESP_LOGW(TAG, "Could not allocate space to read persisted sorter configuration");
        return ESP_OK;
    }
    esp_err_t load_ret = nvs_config_load_sorter_config(saved_config, 4096);
    if (load_ret == ESP_OK) {
        bool applied = false;
        char reason[128] = {0};
        s_loading_persisted_config = true;
        esp_err_t apply_ret = sorting_engine_apply_config_json(saved_config, &applied,
                                                                reason, sizeof(reason));
        s_loading_persisted_config = false;
        if (apply_ret != ESP_OK) {
            ESP_LOGW(TAG, "Stored sorter configuration rejected; using defaults: %s", reason);
        } else {
            ESP_LOGI(TAG, "Restored persisted sorter configuration (applied=%s)",
                     applied ? "true" : "false");
        }
    } else if (load_ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Could not load persisted sorter configuration: %s",
                 esp_err_to_name(load_ret));
    }
    free(saved_config);
    return ESP_OK;
}

esp_err_t sorting_engine_on_s1_entry(int64_t encoder_count, int64_t timestamp_us)
{
    if (s_sorting_mutex == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = ESP_ERR_NO_MEM;
    bool tracking_overflow = false;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);

    // Chống dội / Khóa khoảng cách tối thiểu giữa 2 lần phát hiện vật cản:
    // Nếu một xung S1 mới xuất hiện khi vật trước đó vừa vào chưa đi được 8cm (80mm)
    // HOẶC thời gian cách nhau chưa đủ 800ms -> Coi đây là dội/nhiễu qua lỗ hổng của cùng 1 vật cản và bỏ qua.
    if (s_has_previous_entry) {
        int64_t delta_counts = llabs(encoder_count - s_last_entry_encoder);
        float delta_dist_mm = encoder_counts_to_mm(delta_counts);
        int64_t delta_time_us = timestamp_us - s_last_entry_timestamp_us;

        if (delta_dist_mm < 80.0f || delta_time_us < 800000LL) {
            ESP_LOGW(TAG, "Duplicate S1 trigger filtered out (dist: %.1f mm < 80mm, dt: %lld ms) -> Same item, ignored",
                     delta_dist_mm, delta_time_us / 1000);
            xSemaphoreGive(s_sorting_mutex);
            return ESP_OK;
        }
    }

    if (!s_encoder_sample_valid) {
        s_last_encoder_sample = encoder_count;
        s_encoder_sample_valid = true;
    }

    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        if (s_items[i].state == ITEM_STATE_FREE) {
            tracked_item_t *item = &s_items[i];
            memset(item, 0, sizeof(tracked_item_t));
            item->id = ++s_item_seq_counter;
            snprintf(item->product_id, sizeof(item->product_id), "prod_%lu", item->id);
            item->entry_encoder = encoder_count;
            item->entry_timestamp_us = timestamp_us;
            item->routed_bin = 0;
            item->actual_bin = 0;
            item->triggered = false;
            item->travelled_counts = 0;
            item->pause_start_us = 0;
            item->at_camera_scanned = false;
            item->scanning_now = false;
            item->actuating_now = false;
            item->moving = true;
            item->vision_assigned = false;
            item->state = ITEM_STATE_NEW;
            item->target_bin = 0;
            item->confidence = 0.0f;
            strncpy(item->status, "in_transit", sizeof(item->status) - 1);

            s_last_entry_encoder = encoder_count;
            s_last_entry_timestamp_us = timestamp_us;
            s_has_previous_entry = true;

            ESP_LOGI(TAG, "New item #%lu detected at S1 (0cm) -> Conveyor running continuous (Encoder: %lld)",
                     item->id, encoder_count);
            ret = ESP_OK;
            break;
        }
    }

    if (ret != ESP_OK) {
        s_tracking_overflow_latched = true;
        tracking_overflow = true;
    }

    xSemaphoreGive(s_sorting_mutex);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Tracking queue full! Item at S1 was lost; latching stop until operator clears belt.");
        if (tracking_overflow) state_machine_process_event(SYSTEM_EVENT_STOP);
    }
    return ret;
}

void sorting_engine_note_tracking_overflow(void)
{
    if (s_sorting_mutex == NULL) return;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);
    s_tracking_overflow_latched = true;
    xSemaphoreGive(s_sorting_mutex);
    ESP_LOGE(TAG, "S1 queue overflow latched; operator inspection is required before restart");
    state_machine_process_event(SYSTEM_EVENT_STOP);
}

esp_err_t sorting_engine_on_vision_result(const char *product_id, const char *brand_id, float confidence)
{
    if (s_sorting_mutex == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);

    // Tìm vật thể cũ nhất chưa được gán nhãn Vision (theo thứ tự FIFO - vào trước ra trước)
    tracked_item_t *oldest_new_item = NULL;
    int64_t oldest_time = INT64_MAX;

    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        if (s_items[i].state != ITEM_STATE_FREE &&
            s_items[i].state != ITEM_STATE_COMPLETED &&
            !s_items[i].vision_assigned) {
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
        oldest_new_item->vision_assigned = true;
        oldest_new_item->state = ITEM_STATE_IN_TRANSIT;
        strncpy(oldest_new_item->status, "in_transit", sizeof(oldest_new_item->status) - 1);

        ESP_LOGI(TAG, "Item #%lu paired with DroidCam Vision: product='%s', brand='%s', conf=%.2f -> Target Bin %d",
                 oldest_new_item->id, oldest_new_item->product_id, oldest_new_item->brand_id,
                 confidence, oldest_new_item->target_bin);
        ret = ESP_OK;
    } else {
        ESP_LOGW(TAG, "Vision result received but no pending unassigned item in queue!");
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

    float dist_s1_camera = DIST_S1_TO_CAMERA_MM;
    float dist_s1_servo1 = (sys_cfg->dist_s1_servo1_mm > 0.0f) ? sys_cfg->dist_s1_servo1_mm : DIST_S1_TO_SERVO1_MM;
    float dist_s1_servo2 = (sys_cfg->dist_s1_servo2_mm > 0.0f) ? sys_cfg->dist_s1_servo2_mm : DIST_S1_TO_SERVO2_MM;
    float dist_s1_end    = (sys_cfg->dist_s1_end_mm > 0.0f) ? sys_cfg->dist_s1_end_mm : DIST_S1_TO_END_MM;

    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);

    if (!s_encoder_sample_valid) {
        s_last_encoder_sample = current_encoder;
        s_encoder_sample_valid = true;
    }
    int64_t delta_counts = llabs(current_encoder - s_last_encoder_sample);
    s_last_encoder_sample = current_encoder;

    // Khi hệ thống đang ở chế độ chờ IDLE / STOPPED / ESTOP / JAMMED:
    // Không thực thi điều khiển băng tải hoặc Servo tự động (tránh xung đột với lệnh Test thủ công từ Web)
    if (state_machine_get_state() != SYSTEM_STATE_RUNNING) {
        if (s_pending_config.pending_apply) {
            s_active_config = s_pending_config;
            s_pending_config.pending_apply = false;
            ESP_LOGI(TAG, "Applied pending bin routing config (v%lu) while in standby",
                     s_active_config.config_version);
        }
        xSemaphoreGive(s_sorting_mutex);
        return;
    }

    // -------------------------------------------------------------------------
    // BƯỚC 1: Kiểm tra các vật đang chờ Servo 1/2 quay hết chu trình 4 vòng thuận + 4 vòng ngược
    // -------------------------------------------------------------------------
    bool any_actuating_now = false;

    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        tracked_item_t *item = &s_items[i];
        if (item->state == ITEM_STATE_FREE) continue;

        if (item->actuating_now) {
            if (item->target_bin == 1) {
                if (!servo_is_busy(SERVO_ID_1)) {
                    item->actuating_now = false;
                    item->state = ITEM_STATE_COMPLETED;
                    item->actual_bin = 1;
                    strncpy(item->status, "success", sizeof(item->status) - 1);
                    ESP_LOGI(TAG, "Item #%lu: Servo 1 finished 4 CW + 4 CCW turns -> Bin 1 complete, conveyor resuming!",
                             item->id);
                } else {
                    any_actuating_now = true;
                }
            } else if (item->target_bin == 2) {
                if (!servo_is_busy(SERVO_ID_2)) {
                    item->actuating_now = false;
                    item->state = ITEM_STATE_COMPLETED;
                    item->actual_bin = 2;
                    strncpy(item->status, "success", sizeof(item->status) - 1);
                    ESP_LOGI(TAG, "Item #%lu: Servo 2 finished 4 CW + 4 CCW turns -> Bin 2 complete, conveyor resuming!",
                             item->id);
                } else {
                    any_actuating_now = true;
                }
            } else {
                if ((now_us - item->pause_start_us) >= BIN3_DROP_HOLD_US) {
                    item->actuating_now = false;
                    item->state = ITEM_STATE_COMPLETED;
                    item->actual_bin = 3;
                    strncpy(item->status, "success", sizeof(item->status) - 1);
                    ESP_LOGI(TAG, "Item #%lu: Dropped into Bin 3 (65cm) complete!", item->id);
                } else {
                    any_actuating_now = true;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // BƯỚC 2: Cập nhật vị trí encoder cho các vật thể trên băng tải
    // BĂNG TẢI CHẠY LIÊN TỤC:
    // - Vật trôi qua buồng DroidCam (10cm) KHÔNG DỪNG (AI quét on-the-fly).
    // - Chỉ dừng băng tải khi vật tới Ô chứa 1 (25cm) hoặc Ô chứa 2 (45cm) để Servo gạt hàng.
    // -------------------------------------------------------------------------
    bool is_conveyor_stopped_by_actuator = any_actuating_now ||
                                          servo_is_busy(SERVO_ID_1) ||
                                          servo_is_busy(SERVO_ID_2);

    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        tracked_item_t *item = &s_items[i];
        if (item->state == ITEM_STATE_FREE || item->state == ITEM_STATE_COMPLETED) continue;
        if (item->actuating_now) continue;

        // Chỉ tích lũy quãng đường khi băng tải đang di chuyển
        if (!is_conveyor_stopped_by_actuator) {
            item->travelled_counts += delta_counts;
        }
        float dist_mm = encoder_counts_to_mm(item->travelled_counts);

        // Vị trí buồng DroidCam (10cm = 100mm): Chạy liên tục, ghi nhận qua buồng quét
        if (dist_mm >= dist_s1_camera && !item->at_camera_scanned) {
            item->at_camera_scanned = true;
            ESP_LOGI(TAG, "Item #%lu passed DroidCam chamber (%.1f mm = 10cm) -> Conveyor running continuous",
                     item->id, dist_mm);
        }

        // Nếu vật sắp tới vị trí Servo 1 (25cm) mà AI chưa gửi kết quả qua MQTT -> Đẩy thẳng về Khay mặc định (Khay 3), KHÔNG xoay vòng 1-2-3
        if (!item->vision_assigned && dist_mm >= (dist_s1_servo1 - 15.0f)) {
#if SORTIX_FALLBACK_POLICY == FALLBACK_POLICY_PAUSE_AND_HOLD
            if (!item->reported_error) {
                item->reported_error = true;
                item->state = ITEM_STATE_TIMEOUT;
                state_machine_process_event(SYSTEM_EVENT_STOP);
                ESP_LOGW(TAG, "Item #%lu: Vision timeout under PAUSE_AND_HOLD -> stopped belt", item->id);
            }
#else
            item->target_bin = s_active_config.default_bin;
            strncpy(item->brand_id, "unclassified", sizeof(item->brand_id) - 1);
            item->confidence = 0.0f;
            item->vision_assigned = true;
            strncpy(item->status, "diverted_default", sizeof(item->status) - 1);
            ESP_LOGW(TAG, "Item #%lu near Servo 1 (%.1f mm) without AI classification -> Diverted to Default Bin %d",
                     item->id, dist_mm, item->target_bin);
#endif
        }

        // Vị trí Ô chứa 1: Servo 1 (25cm = 250mm)
        if (item->target_bin == 1) {
            if (dist_mm >= dist_s1_servo1 && !item->triggered) {
                motor_control_brake();
                esp_err_t s_err = servo_trigger(SERVO_ID_1);
                if (s_err == ESP_OK) {
                    item->triggered = true;
                    item->actuating_now = true;
                    item->routed_bin = 1;
                    strncpy(item->status, "unverified", sizeof(item->status) - 1);
                    any_actuating_now = true;
                    is_conveyor_stopped_by_actuator = true;
                    ESP_LOGI(TAG, "Item #%lu reached Bin 1 (%.1f mm / 25cm) -> Conveyor BRAKED, Servo 1 rotating 4 CW + 4 CCW turns",
                             item->id, dist_mm);
                } else {
                    item->servo_failed = true;
                }
            }
        }
        // Vị trí Ô chứa 2: Servo 2 (40cm = 400mm)
        else if (item->target_bin == 2) {
            if (dist_mm >= dist_s1_servo2 && !item->triggered) {
                motor_control_brake();
                esp_err_t s_err = servo_trigger(SERVO_ID_2);
                if (s_err == ESP_OK) {
                    item->triggered = true;
                    item->actuating_now = true;
                    item->routed_bin = 2;
                    strncpy(item->status, "unverified", sizeof(item->status) - 1);
                    any_actuating_now = true;
                    is_conveyor_stopped_by_actuator = true;
                    ESP_LOGI(TAG, "Item #%lu reached Bin 2 (%.1f mm / %.0fcm) -> Conveyor BRAKED, Servo 2 rotating 4 CW + 4 CCW turns",
                             item->id, dist_mm, dist_s1_servo2 / 10.0f);
                } else {
                    item->servo_failed = true;
                }
            }
        }
        // Vị trí Ô chứa 3: Cuối băng tải (65cm = 650mm)
        else {
            if (dist_mm >= dist_s1_end && !item->triggered) {
                item->triggered = true;
                item->routed_bin = 3;
                item->actual_bin = 3;
                item->state = ITEM_STATE_COMPLETED;
                if (strcmp(item->status, "diverted_default") != 0) {
                    strncpy(item->status, "success", sizeof(item->status) - 1);
                }
                ESP_LOGI(TAG, "Item #%lu reached Bin 3 (%.1f mm / 65cm) -> Dropped into Bin 3 (status=%s)",
                         item->id, dist_mm, item->status);
            }
        }
    }

    // -------------------------------------------------------------------------
    // BƯỚC 3: Xử lý các vật thể đã hoàn tất chu trình phân loại
    // -------------------------------------------------------------------------
    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        tracked_item_t *item = &s_items[i];
        if (item->state == ITEM_STATE_COMPLETED) {
            if (s_classification_cb != NULL) {
                s_classification_cb(item, s_classification_user_ctx);
            }
            item->state = ITEM_STATE_FREE;
        }
    }

    // -------------------------------------------------------------------------
    // BƯỚC 4: Điều khiển đóng/ngắt băng tải khi hệ thống đang ở SYSTEM_STATE_RUNNING
    // - Khi Servo 1 hoặc Servo 2 đang quay 4 vòng thuận + 4 vòng ngược để gạt hàng:
    //   BĂNG TẢI DỪNG LẠI để chống dồn hàng và chống va chạm.
    // - Khi Servo đã gạt xong và rút về an toàn:
    //   BĂNG TẢI CHẠY TIẾP LIÊN TỤC!
    // -------------------------------------------------------------------------
    static bool s_was_paused_by_servo = false;
    if (state_machine_get_state() == SYSTEM_STATE_RUNNING) {
        bool must_pause_belt = any_actuating_now ||
                               servo_is_busy(SERVO_ID_1) ||
                               servo_is_busy(SERVO_ID_2);
        if (must_pause_belt) {
            if (motor_control_get_direction() != MOTOR_DIR_BRAKE) {
                motor_control_brake();
                ESP_LOGI(TAG, "Diverter servo active -> Conveyor temporarily paused for safe sorting");
            }
            s_was_paused_by_servo = true;
        } else {
            if (!motor_control_is_running()) {
                motor_control_forward();
                if (s_was_paused_by_servo) {
                    ESP_LOGI(TAG, "Diverter servo returned to rest -> Conveyor resumed forward continuous");
                    s_was_paused_by_servo = false;
                }
            }
        }
    } else {
        s_was_paused_by_servo = false;
    }

    // -------------------------------------------------------------------------
    // BƯỚC 6: Áp dụng cấu hình chờ (pending config) khi băng tải trống
    // -------------------------------------------------------------------------
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
    if (out_reason != NULL && reason_max_len > 0) out_reason[0] = '\0';

    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        set_reason(out_reason, reason_max_len, "%s", "Invalid JSON format");
        return ESP_FAIL;
    }

    cJSON *cfg_ver = cJSON_GetObjectItem(root, "config_version");
    cJSON *bins_arr = cJSON_GetObjectItem(root, "bins");
    cJSON *apply_mode = cJSON_GetObjectItem(root, "apply_mode");

    if (!cJSON_IsNumber(cfg_ver) || !isfinite(cfg_ver->valuedouble) ||
        cfg_ver->valuedouble < 1.0 || cfg_ver->valuedouble > UINT32_MAX ||
        floor(cfg_ver->valuedouble) != cfg_ver->valuedouble) {
        set_reason(out_reason, reason_max_len, "%s", "config_version must be a positive 32-bit integer");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    if (!cJSON_IsArray(bins_arr)) {
        set_reason(out_reason, reason_max_len, "%s", "Missing or invalid bins array");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int arr_size = cJSON_GetArraySize(bins_arr);
    if (arr_size < 1 || arr_size > 2) {
        set_reason(out_reason, reason_max_len, "Invalid bins count (%d). Exactly 1 or 2 bins supported.", arr_size);
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    dynamic_sorter_config_t new_cfg;
    memset(&new_cfg, 0, sizeof(dynamic_sorter_config_t));
    new_cfg.config_version = (uint32_t)cfg_ver->valuedouble;
    new_cfg.default_bin = 3;
    new_cfg.bin_rule_count = 0;

    bool seen_bin[3] = { false, false, false };

    for (int i = 0; i < arr_size; i++) {
        cJSON *bin_obj = cJSON_GetArrayItem(bins_arr, i);
        if (!cJSON_IsObject(bin_obj)) {
            set_reason(out_reason, reason_max_len, "%s", "Bin element must be an object");
            cJSON_Delete(root);
            return ESP_FAIL;
        }

        cJSON *bin_id = cJSON_GetObjectItem(bin_obj, "bin_id");
        cJSON *brands = cJSON_GetObjectItem(bin_obj, "brand_ids");

        if (!cJSON_IsNumber(bin_id) || !isfinite(bin_id->valuedouble) ||
            floor(bin_id->valuedouble) != bin_id->valuedouble ||
            (bin_id->valuedouble != 1.0 && bin_id->valuedouble != 2.0)) {
            set_reason(out_reason, reason_max_len, "bin_id must be integer 1 or 2 (got %.0f)",
                       cJSON_IsNumber(bin_id) ? bin_id->valuedouble : -1.0);
            cJSON_Delete(root);
            return ESP_FAIL;
        }

        int b_id = (int)bin_id->valuedouble;
        if (seen_bin[b_id]) {
            set_reason(out_reason, reason_max_len, "Duplicate bin_id %d in configuration", b_id);
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        seen_bin[b_id] = true;

        if (!cJSON_IsArray(brands)) {
            set_reason(out_reason, reason_max_len, "Bin %d must have brand_ids array", b_id);
            cJSON_Delete(root);
            return ESP_FAIL;
        }

        int b_size = cJSON_GetArraySize(brands);
        if (b_size < 1 || b_size > SORTING_MAX_BRAND_IDS) {
            set_reason(out_reason, reason_max_len, "Bin %d brand_ids count must be 1-%d (got %d)", b_id, SORTING_MAX_BRAND_IDS, b_size);
            cJSON_Delete(root);
            return ESP_FAIL;
        }

        bin_rule_t *rule = &new_cfg.bins[new_cfg.bin_rule_count++];
        rule->bin_id = b_id;
        rule->brand_count = 0;

        for (int j = 0; j < b_size; j++) {
            cJSON *b_item = cJSON_GetArrayItem(brands, j);
            if (!cJSON_IsString(b_item) || b_item->valuestring == NULL ||
                strlen(b_item->valuestring) == 0 || strlen(b_item->valuestring) >= SORTING_MAX_BRAND_LEN) {
                set_reason(out_reason, reason_max_len, "Brand ID invalid or exceeds max length (%d)", SORTING_MAX_BRAND_LEN - 1);
                cJSON_Delete(root);
                return ESP_FAIL;
            }
            bool duplicate_brand = false;
            for (int prior_rule = 0; prior_rule < new_cfg.bin_rule_count && !duplicate_brand; prior_rule++) {
                const bin_rule_t *prior = &new_cfg.bins[prior_rule];
                for (int prior_brand = 0; prior_brand < prior->brand_count; prior_brand++) {
                    if (strcasecmp(prior->brand_ids[prior_brand], b_item->valuestring) == 0) {
                        duplicate_brand = true;
                        break;
                    }
                }
            }
            if (duplicate_brand) {
                set_reason(out_reason, reason_max_len, "Brand ID '%s' is mapped more than once", b_item->valuestring);
                cJSON_Delete(root);
                return ESP_FAIL;
            }
            strncpy(rule->brand_ids[rule->brand_count], b_item->valuestring, SORTING_MAX_BRAND_LEN - 1);
            rule->brand_ids[rule->brand_count][SORTING_MAX_BRAND_LEN - 1] = '\0';
            rule->brand_count++;
        }
    }

    bool immediate = false;
    if (apply_mode != NULL) {
        if (!cJSON_IsString(apply_mode)) {
            set_reason(out_reason, reason_max_len, "%s", "apply_mode must be a string");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        if (strcmp(apply_mode->valuestring, "immediate") == 0) {
            immediate = true;
        } else if (strcmp(apply_mode->valuestring, "drained") == 0 || strcmp(apply_mode->valuestring, "when_line_empty") == 0) {
            immediate = false;
        } else {
            set_reason(out_reason, reason_max_len, "Invalid apply_mode '%s' (must be 'immediate', 'drained' or 'when_line_empty')", apply_mode->valuestring);
            cJSON_Delete(root);
            return ESP_FAIL;
        }
    }

    if (!s_loading_persisted_config) {
        // Tuyệt đối không lưu token xác thực MQTT cùng với các luật phân loại vào bộ nhớ NVS
        cJSON_DeleteItemFromObject(root, "auth_token");
        cJSON_DeleteItemFromObject(root, "token");
        char *persisted_json = cJSON_PrintUnformatted(root);
        if (persisted_json == NULL) {
            set_reason(out_reason, reason_max_len, "%s", "Could not serialize sorter configuration");
            cJSON_Delete(root);
            return ESP_ERR_NO_MEM;
        }
        esp_err_t persist_ret = nvs_config_save_sorter_config(persisted_json);
        free(persisted_json);
        if (persist_ret != ESP_OK) {
            set_reason(out_reason, reason_max_len, "Could not persist sorter configuration: %s",
                       esp_err_to_name(persist_ret));
            cJSON_Delete(root);
            return persist_ret;
        }
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
    bool empty = !s_tracking_overflow_latched;
    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        if (s_items[i].state != ITEM_STATE_FREE) {
            empty = false;
            break;
        }
    }
    xSemaphoreGive(s_sorting_mutex);
    return empty;
}

bool sorting_engine_has_unresolved_items(void)
{
    if (s_sorting_mutex == NULL) return false;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);
    bool has_unresolved = s_tracking_overflow_latched;
    if (!has_unresolved) {
        for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
            if (s_items[i].state == ITEM_STATE_TIMEOUT || s_items[i].servo_failed) {
                has_unresolved = true;
                break;
            }
        }
    }
    xSemaphoreGive(s_sorting_mutex);
    return has_unresolved;
}

esp_err_t sorting_engine_clear_unresolved(void)
{
    if (s_sorting_mutex == NULL) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);
    int cleared_count = 0;
    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        if (s_items[i].state == ITEM_STATE_TIMEOUT || s_items[i].servo_failed) {
            s_items[i].state = ITEM_STATE_FREE;
            cleared_count++;
        }
    }
    s_tracking_overflow_latched = false;
    xSemaphoreGive(s_sorting_mutex);
    ESP_LOGI(TAG, "Cleared %d unresolved items from tracking queue", cleared_count);
    return ESP_OK;
}

void sorting_engine_reset(void)
{
    if (s_sorting_mutex == NULL) return;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);
    for (int i = 0; i < SORTING_MAX_TRACKED_ITEMS; i++) {
        s_items[i].state = ITEM_STATE_FREE;
    }
    s_tracking_overflow_latched = false;
    s_last_encoder_sample = encoder_get_count();
    s_encoder_sample_valid = true;
    s_has_previous_entry = false;
    xSemaphoreGive(s_sorting_mutex);
    ESP_LOGI(TAG, "Sorting engine tracking queue reset");
}

uint32_t sorting_engine_get_active_config_version(void)
{
    if (s_sorting_mutex == NULL) return s_active_config.config_version;
    xSemaphoreTake(s_sorting_mutex, portMAX_DELAY);
    uint32_t version = s_active_config.config_version;
    xSemaphoreGive(s_sorting_mutex);
    return version;
}
