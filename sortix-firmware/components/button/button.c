#include "button.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "BUTTON_START";

#define BUTTON_PIN_START            GPIO_NUM_23
#define BTN_POLL_PERIOD_US          10000   // Lấy mẫu chu kỳ 10ms (100 Hz)
#define BTN_PRESS_STABLE_SAMPLES    3       // 3 mẫu liên tiếp = 30ms ổn định LOW để xác nhận NHẤN
#define BTN_RELEASE_STABLE_SAMPLES  3       // 3 mẫu liên tiếp = 30ms ổn định HIGH để xác nhận NHẢ

static button_cb_t s_on_pressed_cb = NULL;
static void *s_user_ctx = NULL;
static esp_timer_handle_t s_poll_timer = NULL;

static volatile int s_low_counter = 0;
static volatile int s_high_counter = 0;
static volatile bool s_is_pressed = false;

static void button_poll_timer_cb(void *arg)
{
    int pin_level = gpio_get_level(BUTTON_PIN_START);

    if (pin_level == 0) {
        // Nút nhấn nối GND: Đang ở mức LOW
        s_high_counter = 0;
        if (!s_is_pressed) {
            s_low_counter++;
            if (s_low_counter >= BTN_PRESS_STABLE_SAMPLES) {
                s_is_pressed = true;
                s_low_counter = 0;
                ESP_LOGI(TAG, "Start button press detected (Software Debounced 30ms, NO RC) -> Dispatching event");
                if (s_on_pressed_cb != NULL) {
                    s_on_pressed_cb(s_user_ctx);
                }
            }
        }
    } else {
        // Nút nhả: Trở kéo nội Pull-up kéo lên mức HIGH
        s_low_counter = 0;
        if (s_is_pressed) {
            s_high_counter++;
            if (s_high_counter >= BTN_RELEASE_STABLE_SAMPLES) {
                s_is_pressed = false;
                s_high_counter = 0;
                ESP_LOGD(TAG, "Start button released -> Ready for next press");
            }
        }
    }
}

esp_err_t button_init(button_cb_t on_pressed_cb, void *user_ctx)
{
    s_on_pressed_cb = on_pressed_cb;
    s_user_ctx = user_ctx;
    s_low_counter = 0;
    s_high_counter = 0;
    s_is_pressed = false;

    ESP_LOGI(TAG, "Initializing Start Button GPIO%d (Pure Software Debounce 10ms polling, NO RC filter required)",
             BUTTON_PIN_START);

    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN_START),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,       // Bật trở kéo lên nội 45k cho tiếp điểm phím cơ
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,         // Không dùng ngắt, dùng lấy mẫu phần mềm chống rung tuyệt đối
    };
    esp_err_t ret = gpio_config(&btn_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure Start Button GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &button_poll_timer_cb,
        .name = "btn_poll_timer"
    };
    ret = esp_timer_create(&timer_args, &s_poll_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create button poll timer: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_timer_start_periodic(s_poll_timer, BTN_POLL_PERIOD_US);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start button poll timer: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}

int button_get_level(void)
{
    return gpio_get_level(BUTTON_PIN_START);
}
