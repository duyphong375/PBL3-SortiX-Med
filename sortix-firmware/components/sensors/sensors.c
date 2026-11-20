#include "sensors.h"
#include "encoder.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "SENSORS";

#define SENSOR_PIN_S1               GPIO_NUM_3
#define SENSOR_PIN_LDR              GPIO_NUM_5

#define S1_POLL_PERIOD_US           5000    // Lấy mẫu định kỳ 5ms (200 Hz) không cần mạch lọc RC
#define S1_BLOCKED_STABLE_SAMPLES   3       // 3 mẫu LOW liên tiếp (15ms) xác nhận có vật cản thật
#define S1_CLEAR_STABLE_SAMPLES     25      // 25 mẫu HIGH liên tiếp (125ms) xác nhận vật đã đi qua hoàn toàn, chống dội qua lỗ hở của dụng cụ
#define S1_STARTUP_GRACE_US         1500000 // Khoảng trễ an toàn 1.5s chống kích hoạt giả lúc cấp nguồn/reset

static QueueHandle_t s_event_queue = NULL;
static esp_timer_handle_t s_s1_poll_timer = NULL;
static volatile int s_s1_low_cnt = 0;
static volatile int s_s1_high_cnt = 0;
static volatile bool s_s1_is_latched = false;
static volatile bool s_s1_queue_overflow = false;
static portMUX_TYPE s_s1_overflow_mux = portMUX_INITIALIZER_UNLOCKED;

static void s1_poll_timer_cb(void *arg)
{
    int64_t now_us = esp_timer_get_time();
    if (now_us < S1_STARTUP_GRACE_US) {
        return;
    }

    int level = gpio_get_level(SENSOR_PIN_S1);
    if (level == 0) {
        // Cảm biến tích cực mức thấp (0 = có vật cản che)
        s_s1_high_cnt = 0;
        if (!s_s1_is_latched) {
            s_s1_low_cnt++;
            if (s_s1_low_cnt >= S1_BLOCKED_STABLE_SAMPLES) {
                s_s1_is_latched = true;
                s_s1_low_cnt = 0;

                s1_event_t evt = {
                    .encoder_count = encoder_get_count(),
                    .timestamp_us = now_us,
                };

                if (s_event_queue != NULL) {
                    if (xQueueSend(s_event_queue, &evt, 0) != pdTRUE) {
                        portENTER_CRITICAL(&s_s1_overflow_mux);
                        s_s1_queue_overflow = true;
                        portEXIT_CRITICAL(&s_s1_overflow_mux);
                    }
                }
            }
        }
    } else {
        // Mức cao (1 = thông thoáng)
        s_s1_low_cnt = 0;
        if (s_s1_is_latched) {
            s_s1_high_cnt++;
            if (s_s1_high_cnt >= S1_CLEAR_STABLE_SAMPLES) {
                s_s1_is_latched = false;
                s_s1_high_cnt = 0;
            }
        }
    }
}

esp_err_t sensors_init(QueueHandle_t event_queue)
{
    s_event_queue = event_queue;
    s_s1_low_cnt = 0;
    s_s1_high_cnt = 0;
    s_s1_is_latched = false;
    ESP_LOGI(TAG, "Initializing Sensors: S1 (GPIO%d, Software Debounce 15ms NO-RC), LDR (GPIO%d)",
             SENSOR_PIN_S1, SENSOR_PIN_LDR);

    // Cảm biến hồng ngoại S1 (GPIO3): Đầu vào, Điện trở kéo lên (Pull-up), Lọc nhiễu phần mềm không dùng ngắt
    gpio_config_t s1_conf = {
        .pin_bit_mask = (1ULL << SENSOR_PIN_S1),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&s1_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure S1 GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // Cảm biến quang trở LDR (GPIO5): Đầu vào số DO, Điện trở kéo lên (Pull-up)
    gpio_config_t ldr_conf = {
        .pin_bit_mask = (1ULL << SENSOR_PIN_LDR),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&ldr_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LDR GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &s1_poll_timer_cb,
        .name = "s1_poll_timer"
    };
    ret = esp_timer_create(&timer_args, &s_s1_poll_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create S1 poll timer: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = esp_timer_start_periodic(s_s1_poll_timer, S1_POLL_PERIOD_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start S1 poll timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Sensors initialized successfully (Pure Software Debounce active)");
    return ESP_OK;
}

bool sensors_is_s1_blocked(void)
{
    // Bỏ qua và trả về false trong 1.5s đầu để tránh gửi telemetry s1_entry=true gây bật Dashboard
    if (esp_timer_get_time() < S1_STARTUP_GRACE_US) {
        return false;
    }
    // Cảm biến tích cực mức thấp (Active Low): 0 = có vật thể che, 1 = thông thoáng
    return (gpio_get_level(SENSOR_PIN_S1) == 0);
}

int sensors_get_ldr_level(void)
{
    return gpio_get_level(SENSOR_PIN_LDR);
}

bool sensors_take_s1_queue_overflow(void)
{
    portENTER_CRITICAL(&s_s1_overflow_mux);
    bool overflowed = s_s1_queue_overflow;
    s_s1_queue_overflow = false;
    portEXIT_CRITICAL(&s_s1_overflow_mux);
    return overflowed;
}

void sensors_rearm_s1(void)
{
    s_s1_low_cnt = 0;
    s_s1_high_cnt = 0;
    s_s1_is_latched = false;
}
