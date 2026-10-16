#include "servo.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SERVO_MG996R";

#define SERVO_PIN_1             GPIO_NUM_6
#define SERVO_PIN_2             GPIO_NUM_24

#define LEDC_SERVO_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_SERVO_TIMER        LEDC_TIMER_0
#define LEDC_SERVO_RES          LEDC_TIMER_14_BIT
#define LEDC_SERVO_FREQ_HZ      50      // 50Hz = 20,000 us period
#define LEDC_MAX_DUTY           16383   // (2^14 - 1)

typedef struct {
    gpio_num_t pin;
    ledc_channel_t channel;
    servo_fsm_state_t state;
    int64_t state_start_us;
} servo_channel_ctx_t;

static servo_channel_ctx_t s_servos[SERVO_COUNT] = {
    { .pin = SERVO_PIN_1, .channel = LEDC_CHANNEL_0, .state = SERVO_STATE_IDLE, .state_start_us = 0 },
    { .pin = SERVO_PIN_2, .channel = LEDC_CHANNEL_1, .state = SERVO_STATE_IDLE, .state_start_us = 0 }
};

static uint32_t s_rest_us = 1000;       // Rest position ~0 degrees
static uint32_t s_active_us = 2000;     // Divert position ~90 degrees
static uint32_t s_open_time_ms = 200;   // Physical transit time to open
static uint32_t s_hold_time_ms = 600;   // Diverter dwell time
static uint32_t s_close_time_ms = 250;  // Physical transit time to return

static esp_timer_handle_t s_fsm_timer = NULL;
static SemaphoreHandle_t s_servo_mutex = NULL;

static inline uint32_t pulse_to_duty(uint32_t pulse_us)
{
    return (pulse_us * LEDC_MAX_DUTY) / 20000;
}

static void servo_set_pulse_raw(servo_id_t id, uint32_t pulse_us)
{
    uint32_t duty = pulse_to_duty(pulse_us);
    ledc_set_duty(LEDC_SERVO_MODE, s_servos[id].channel, duty);
    ledc_update_duty(LEDC_SERVO_MODE, s_servos[id].channel);
}

static void servo_fsm_timer_cb(void *arg)
{
    int64_t now_us = esp_timer_get_time();

    if (xSemaphoreTake(s_servo_mutex, 0) == pdTRUE) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            servo_channel_ctx_t *ctx = &s_servos[i];
            int64_t elapsed_ms = (now_us - ctx->state_start_us) / 1000;

            switch (ctx->state) {
                case SERVO_STATE_IDLE:
                    // Nothing to do
                    break;

                case SERVO_STATE_OPENING:
                    if (elapsed_ms >= s_open_time_ms) {
                        ctx->state = SERVO_STATE_HOLDING;
                        ctx->state_start_us = now_us;
                        ESP_LOGD(TAG, "Servo %d reached HOLDING", i + 1);
                    }
                    break;

                case SERVO_STATE_HOLDING:
                    if (elapsed_ms >= s_hold_time_ms) {
                        ctx->state = SERVO_STATE_CLOSING;
                        ctx->state_start_us = now_us;
                        servo_set_pulse_raw((servo_id_t)i, s_rest_us);
                        ESP_LOGD(TAG, "Servo %d returning to REST (CLOSING)", i + 1);
                    }
                    break;

                case SERVO_STATE_CLOSING:
                    if (elapsed_ms >= s_close_time_ms) {
                        ctx->state = SERVO_STATE_IDLE;
                        ctx->state_start_us = now_us;
                        ESP_LOGI(TAG, "Servo %d divert cycle COMPLETE (IDLE)", i + 1);
                    }
                    break;
            }
        }
        xSemaphoreGive(s_servo_mutex);
    }
}

esp_err_t servo_init(void)
{
    ESP_LOGI(TAG, "Initializing Servos: S1 on GPIO%d, S2 on GPIO%d", SERVO_PIN_1, SERVO_PIN_2);

    if (s_servo_mutex == NULL) {
        s_servo_mutex = xSemaphoreCreateMutex();
        if (s_servo_mutex == NULL) return ESP_ERR_NO_MEM;
    }

    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_SERVO_MODE,
        .timer_num = LEDC_SERVO_TIMER,
        .duty_resolution = LEDC_SERVO_RES,
        .freq_hz = LEDC_SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        ledc_channel_config_t chan_conf = {
            .speed_mode = LEDC_SERVO_MODE,
            .channel = s_servos[i].channel,
            .timer_sel = LEDC_SERVO_TIMER,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = s_servos[i].pin,
            .duty = pulse_to_duty(s_rest_us),
            .hpoint = 0
        };
        ret = ledc_channel_config(&chan_conf);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure LEDC channel %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
        s_servos[i].state = SERVO_STATE_IDLE;
    }

    // Start 20ms non-blocking FSM timer
    const esp_timer_create_args_t timer_args = {
        .callback = &servo_fsm_timer_cb,
        .name = "servo_fsm_timer"
    };
    ret = esp_timer_create(&timer_args, &s_fsm_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create servo FSM timer: %s", esp_err_to_name(ret));
        return ret;
    }
    esp_timer_start_periodic(s_fsm_timer, 20000); // 20 ms period

    ESP_LOGI(TAG, "Servos initialized at rest position (duty=%lu)", pulse_to_duty(s_rest_us));
    return ESP_OK;
}

esp_err_t servo_trigger(servo_id_t id)
{
    if (id >= SERVO_COUNT) return ESP_ERR_INVALID_ARG;
    if (s_servo_mutex == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = ESP_OK;
    xSemaphoreTake(s_servo_mutex, portMAX_DELAY);
    if (s_servos[id].state != SERVO_STATE_IDLE) {
        ESP_LOGW(TAG, "Servo %d is already busy in state %d", id + 1, s_servos[id].state);
        ret = ESP_ERR_INVALID_STATE;
    } else {
        s_servos[id].state = SERVO_STATE_OPENING;
        s_servos[id].state_start_us = esp_timer_get_time();
        servo_set_pulse_raw(id, s_active_us);
        ESP_LOGI(TAG, "Servo %d TRIGGERED -> OPENING", id + 1);
    }
    xSemaphoreGive(s_servo_mutex);

    return ret;
}

void servo_reset_all(void)
{
    if (s_servo_mutex == NULL) return;
    xSemaphoreTake(s_servo_mutex, portMAX_DELAY);
    for (int i = 0; i < SERVO_COUNT; i++) {
        s_servos[i].state = SERVO_STATE_IDLE;
        servo_set_pulse_raw((servo_id_t)i, s_rest_us);
    }
    xSemaphoreGive(s_servo_mutex);
    ESP_LOGI(TAG, "All servos forced to REST position");
}

bool servo_is_busy(servo_id_t id)
{
    if (id >= SERVO_COUNT) return false;
    return (s_servos[id].state != SERVO_STATE_IDLE);
}

servo_fsm_state_t servo_get_state(servo_id_t id)
{
    if (id >= SERVO_COUNT) return SERVO_STATE_IDLE;
    return s_servos[id].state;
}

void servo_set_timing(uint32_t rest_us, uint32_t active_us, uint32_t hold_ms)
{
    xSemaphoreTake(s_servo_mutex, portMAX_DELAY);
    s_rest_us = rest_us;
    s_active_us = active_us;
    s_hold_time_ms = hold_ms;
    xSemaphoreGive(s_servo_mutex);
}
