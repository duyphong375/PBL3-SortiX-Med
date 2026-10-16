#include "sensors.h"
#include "encoder.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "SENSORS";

#define SENSOR_PIN_S1           GPIO_NUM_0
#define SENSOR_PIN_LDR          GPIO_NUM_5

#define S1_DEBOUNCE_MIN_US      80000   // 80 ms lockout to prevent chatter on edge of medical tools

static QueueHandle_t s_event_queue = NULL;
static volatile int64_t s_last_trigger_us = 0;

static void IRAM_ATTR s1_gpio_isr_handler(void *arg)
{
    int64_t now_us = esp_timer_get_time();
    if ((now_us - s_last_trigger_us) >= S1_DEBOUNCE_MIN_US) {
        s_last_trigger_us = now_us;

        s1_event_t evt = {
            .encoder_count = encoder_get_count(),
            .timestamp_us = now_us,
        };

        if (s_event_queue != NULL) {
            BaseType_t high_task_wakeup = pdFALSE;
            xQueueSendFromISR(s_event_queue, &evt, &high_task_wakeup);
            if (high_task_wakeup == pdTRUE) {
                portYIELD_FROM_ISR(high_task_wakeup);
            }
        }
    }
}

esp_err_t sensors_init(QueueHandle_t event_queue)
{
    s_event_queue = event_queue;
    ESP_LOGI(TAG, "Initializing Sensors: S1 (GPIO%d), LDR (GPIO%d)", SENSOR_PIN_S1, SENSOR_PIN_LDR);

    // GPIO0 (S1 IR Sensor): Input, Pull-up, Falling Edge Interrupt
    gpio_config_t s1_conf = {
        .pin_bit_mask = (1ULL << SENSOR_PIN_S1),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    esp_err_t ret = gpio_config(&s1_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure S1 GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    // GPIO5 (LDR Light Sensor DO): Input, Pull-up
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

    // Install ISR service (ESP_INTR_FLAG_IRAM)
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    ret = gpio_isr_handler_add(SENSOR_PIN_S1, s1_gpio_isr_handler, NULL);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to add S1 ISR handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Sensors initialized successfully");
    return ESP_OK;
}

bool sensors_is_s1_blocked(void)
{
    // Active Low sensor: 0 = object detected, 1 = clear
    return (gpio_get_level(SENSOR_PIN_S1) == 0);
}

int sensors_get_ldr_level(void)
{
    return gpio_get_level(SENSOR_PIN_LDR);
}
