#include "encoder.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static const char *TAG = "ENCODER_PCNT";

#define ENCODER_PIN_A           GPIO_NUM_1
#define ENCODER_PIN_B           GPIO_NUM_4
#define PCNT_HIGH_LIMIT         20000
#define PCNT_LOW_LIMIT          -20000

static pcnt_unit_handle_t s_pcnt_unit = NULL;
static pcnt_channel_handle_t s_pcnt_chan_a = NULL;
static pcnt_channel_handle_t s_pcnt_chan_b = NULL;

static portMUX_TYPE s_encoder_spinlock = portMUX_INITIALIZER_UNLOCKED;
static volatile int64_t s_accumulator = 0;
static float s_counts_per_mm = 31.5f;

static bool IRAM_ATTR pcnt_on_reach(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx)
{
    portENTER_CRITICAL_ISR(&s_encoder_spinlock);
    s_accumulator += edata->watch_point_value;
    pcnt_unit_clear_count(unit);
    portEXIT_CRITICAL_ISR(&s_encoder_spinlock);
    return false;
}

esp_err_t encoder_init(void)
{
    ESP_LOGI(TAG, "Initializing PCNT Quadrature Encoder (Phase A: GPIO%d, Phase B: GPIO%d)",
             ENCODER_PIN_A, ENCODER_PIN_B);

    pcnt_unit_config_t unit_config = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
        .flags.accum_count = 1,
    };
    esp_err_t ret = pcnt_new_unit(&unit_config, &s_pcnt_unit);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT unit: %s", esp_err_to_name(ret));
        return ret;
    }

    // Channel A: Edge on Pin A, Level on Pin B
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = ENCODER_PIN_A,
        .level_gpio_num = ENCODER_PIN_B,
    };
    ret = pcnt_new_channel(s_pcnt_unit, &chan_a_config, &s_pcnt_chan_a);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT channel A: %s", esp_err_to_name(ret));
        return ret;
    }

    // Channel B: Edge on Pin B, Level on Pin A
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = ENCODER_PIN_B,
        .level_gpio_num = ENCODER_PIN_A,
    };
    ret = pcnt_new_channel(s_pcnt_unit, &chan_b_config, &s_pcnt_chan_b);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create PCNT channel B: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set edge and level actions for full quadrature x4
    pcnt_channel_set_edge_action(s_pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(s_pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    pcnt_channel_set_edge_action(s_pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(s_pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // Glitch filter (1000ns)
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    pcnt_unit_set_glitch_filter(s_pcnt_unit, &filter_config);

    // Watchpoints for high and low limit overflow
    pcnt_unit_add_watch_point(s_pcnt_unit, PCNT_HIGH_LIMIT);
    pcnt_unit_add_watch_point(s_pcnt_unit, PCNT_LOW_LIMIT);

    pcnt_event_callbacks_t cbs = {
        .on_reach = pcnt_on_reach,
    };
    pcnt_unit_register_event_callbacks(s_pcnt_unit, &cbs, NULL);

    pcnt_unit_enable(s_pcnt_unit);
    pcnt_unit_clear_count(s_pcnt_unit);
    pcnt_unit_start(s_pcnt_unit);

    s_accumulator = 0;
    ESP_LOGI(TAG, "Encoder PCNT started successfully");
    return ESP_OK;
}

int64_t encoder_get_count(void)
{
    if (s_pcnt_unit == NULL) return 0;
    int current_val = 0;
    pcnt_unit_get_count(s_pcnt_unit, &current_val);

    portENTER_CRITICAL(&s_encoder_spinlock);
    int64_t total = s_accumulator + current_val;
    portEXIT_CRITICAL(&s_encoder_spinlock);

    return total;
}

void encoder_reset_count(void)
{
    if (s_pcnt_unit == NULL) return;
    portENTER_CRITICAL(&s_encoder_spinlock);
    s_accumulator = 0;
    pcnt_unit_clear_count(s_pcnt_unit);
    portEXIT_CRITICAL(&s_encoder_spinlock);
    ESP_LOGI(TAG, "Encoder count reset to 0");
}

void encoder_set_counts_per_mm(float counts_per_mm)
{
    if (counts_per_mm > 0.001f) {
        s_counts_per_mm = counts_per_mm;
    }
}

float encoder_get_counts_per_mm(void)
{
    return s_counts_per_mm;
}

float encoder_counts_to_mm(int64_t counts)
{
    if (s_counts_per_mm <= 0.001f) return 0.0f;
    return (float)counts / s_counts_per_mm;
}

int64_t encoder_mm_to_counts(float mm)
{
    return (int64_t)(mm * s_counts_per_mm);
}
