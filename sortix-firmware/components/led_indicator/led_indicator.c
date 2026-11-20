#include "led_indicator.h"
#include "sensors.h"
#include "app_config.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "LED_INDICATOR";

#define LED_PIN_STATUS          GPIO_NUM_10
#define LED_TIMER_PERIOD_US     100000  // Chu kỳ timer 100 ms (10 Hz)

#ifndef LDR_DARK_LEVEL
#define LDR_DARK_LEVEL          1       // Mức 1 = Trời tối (thiếu sáng -> BẬT LED), Mức 0 = Trời sáng (đủ sáng -> TẮT LED)
#endif

#define LDR_DEBOUNCE_TICKS      2       // 2 tick x 100ms = 200ms ổn định để chống chớp nháy quang học

static led_mode_t s_current_mode = LED_MODE_IDLE;
static esp_timer_handle_t s_led_timer = NULL;
static SemaphoreHandle_t s_led_mutex = NULL;
static uint32_t s_tick_counter = 0;

static volatile bool s_is_illuminating = false;
static uint8_t s_dark_ticks = 0;
static uint8_t s_bright_ticks = 0;

static void led_timer_cb(void *arg)
{
    s_tick_counter++;

    if (xSemaphoreTake(s_led_mutex, 0) == pdTRUE) {
        int level = 0;

        if (s_current_mode == LED_MODE_ON) {
            // Lệnh thủ công từ Web/Dashboard: Bật sáng liên tục
            level = 1;
            s_is_illuminating = true;
        } else if (s_current_mode == LED_MODE_JAMMED || s_current_mode == LED_MODE_ERROR) {
            // Tần số 5 Hz = 2 tick (1 tick BẬT, 1 tick TẮT) cảnh báo sự cố khẩn cấp
            level = ((s_tick_counter % 2) == 0) ? 1 : 0;
        } else if (s_current_mode == LED_MODE_OFF || s_current_mode == LED_MODE_IDLE) {
            // Khi chưa kích hoạt START bằng nút GPIO23 (trạng thái IDLE hoặc OFF):
            // Đèn LED GPIO10 TẮT HOÀN TOÀN, không chiếu sáng
            level = 0;
            s_is_illuminating = false;
            s_dark_ticks = 0;
            s_bright_ticks = 0;
        } else if (s_current_mode == LED_MODE_RUNNING) {
            // CHỈ KHI ĐÃ BẤM NÚT GPIO23 START (HỆ THỐNG ĐANG CHẠY RUNNING):
            // Mới kích hoạt tự động chiếu sáng theo cảm biến quang trở LDR
            int ldr_raw = sensors_get_ldr_level();
            bool is_dark = (ldr_raw == LDR_DARK_LEVEL); // Mức 1 = Thiếu sáng (Tối) -> BẬT LED trợ sáng

            if (is_dark) {
                if (s_dark_ticks < LDR_DEBOUNCE_TICKS) {
                    s_dark_ticks++;
                }
                s_bright_ticks = 0;
                if (s_dark_ticks >= LDR_DEBOUNCE_TICKS) {
                    s_is_illuminating = true;
                }
            } else {
                if (s_bright_ticks < LDR_DEBOUNCE_TICKS) {
                    s_bright_ticks++;
                }
                s_dark_ticks = 0;
                if (s_bright_ticks >= LDR_DEBOUNCE_TICKS) {
                    s_is_illuminating = false;
                }
            }

            level = s_is_illuminating ? 1 : 0;
        }

        gpio_set_level(LED_PIN_STATUS, level);
        xSemaphoreGive(s_led_mutex);
    }
}

esp_err_t led_indicator_init(void)
{
    ESP_LOGI(TAG, "Initializing Auto-Illumination LED on GPIO%d (LDR DO on GPIO5, Dark Level=%d)",
             LED_PIN_STATUS, LDR_DARK_LEVEL);

    if (s_led_mutex == NULL) {
        s_led_mutex = xSemaphoreCreateMutex();
        if (s_led_mutex == NULL) return ESP_ERR_NO_MEM;
    }

    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED_PIN_STATUS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t ret = gpio_config(&led_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_set_level(LED_PIN_STATUS, 0);

    const esp_timer_create_args_t timer_args = {
        .callback = &led_timer_cb,
        .name = "led_timer"
    };
    ret = esp_timer_create(&timer_args, &s_led_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_timer_start_periodic(s_led_timer, LED_TIMER_PERIOD_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LED timer: %s", esp_err_to_name(ret));
        return ret;
    }

    s_current_mode = LED_MODE_IDLE;
    return ESP_OK;
}

void led_indicator_set_mode(led_mode_t mode)
{
    if (s_led_mutex == NULL) return;
    xSemaphoreTake(s_led_mutex, portMAX_DELAY);
    s_current_mode = mode;
    xSemaphoreGive(s_led_mutex);
}

led_mode_t led_indicator_get_mode(void)
{
    return s_current_mode;
}

bool led_indicator_is_illuminating(void)
{
    return s_is_illuminating;
}
