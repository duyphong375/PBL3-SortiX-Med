#include "motor_control.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "MOTOR_CTRL";

#define MOTOR_PIN_IN1  GPIO_NUM_8
#define MOTOR_PIN_IN2  GPIO_NUM_9

static SemaphoreHandle_t s_motor_mutex = NULL;
static motor_direction_t s_current_dir = MOTOR_DIR_COAST;

esp_err_t motor_control_init(void)
{
    if (s_motor_mutex == NULL) {
        s_motor_mutex = xSemaphoreCreateMutex();
        if (s_motor_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create motor mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << MOTOR_PIN_IN1) | (1ULL << MOTOR_PIN_IN2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure motor GPIOs: %s", esp_err_to_name(ret));
        return ret;
    }

    // Default to coast / stop
    gpio_set_level(MOTOR_PIN_IN1, 0);
    gpio_set_level(MOTOR_PIN_IN2, 0);
    s_current_dir = MOTOR_DIR_COAST;

    ESP_LOGI(TAG, "Motor control initialized (IN1: GPIO%d, IN2: GPIO%d)", MOTOR_PIN_IN1, MOTOR_PIN_IN2);
    return ESP_OK;
}

esp_err_t motor_control_forward(void)
{
    if (s_motor_mutex == NULL) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_motor_mutex, portMAX_DELAY);
    gpio_set_level(MOTOR_PIN_IN1, 1);
    gpio_set_level(MOTOR_PIN_IN2, 0);
    s_current_dir = MOTOR_DIR_FORWARD;
    xSemaphoreGive(s_motor_mutex);
    ESP_LOGD(TAG, "Motor set to FORWARD");
    return ESP_OK;
}

esp_err_t motor_control_reverse(void)
{
    if (s_motor_mutex == NULL) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_motor_mutex, portMAX_DELAY);
    gpio_set_level(MOTOR_PIN_IN1, 0);
    gpio_set_level(MOTOR_PIN_IN2, 1);
    s_current_dir = MOTOR_DIR_REVERSE;
    xSemaphoreGive(s_motor_mutex);
    ESP_LOGD(TAG, "Motor set to REVERSE");
    return ESP_OK;
}

esp_err_t motor_control_brake(void)
{
    if (s_motor_mutex == NULL) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_motor_mutex, portMAX_DELAY);
    gpio_set_level(MOTOR_PIN_IN1, 1);
    gpio_set_level(MOTOR_PIN_IN2, 1);
    s_current_dir = MOTOR_DIR_BRAKE;
    xSemaphoreGive(s_motor_mutex);
    ESP_LOGD(TAG, "Motor set to BRAKE");
    return ESP_OK;
}

esp_err_t motor_control_stop(void)
{
    if (s_motor_mutex == NULL) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_motor_mutex, portMAX_DELAY);
    gpio_set_level(MOTOR_PIN_IN1, 0);
    gpio_set_level(MOTOR_PIN_IN2, 0);
    s_current_dir = MOTOR_DIR_COAST;
    xSemaphoreGive(s_motor_mutex);
    ESP_LOGD(TAG, "Motor set to COAST/STOP");
    return ESP_OK;
}

bool motor_control_is_running(void)
{
    return (s_current_dir == MOTOR_DIR_FORWARD);
}

motor_direction_t motor_control_get_direction(void)
{
    return s_current_dir;
}
