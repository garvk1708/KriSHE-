#include "max6675.h"
#include "config.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include <math.h>

static const char *TAG = "MAX6675";

/* Tracking for rate of change calculation (dT/dt) and EMA */
static int64_t s_last_timestamp_us = 0;

typedef struct {
    float last_c;
    bool has_prev;
    int rejection_count;
} channel_state_t;

static channel_state_t s_top_state = {0};
static channel_state_t s_mid_state = {0};
static channel_state_t s_bot_state = {0};

esp_err_t max6675_init(void)
{
    ESP_LOGI(TAG, "Initializing MAX6675 driver (SCK: %d, SO: %d, TOP_CS: %d, MID_CS: %d, BOT_CS: %d)",
             MAX6675_SCK_PIN, MAX6675_SO_PIN,
             MAX6675_TOP_CS_PIN, MAX6675_MID_CS_PIN, MAX6675_BOT_CS_PIN);

    /* Configure SCK and CS pins as outputs with pull-up enabled to prevent floating */
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << MAX6675_SCK_PIN) |
                        (1ULL << MAX6675_TOP_CS_PIN) |
                        (1ULL << MAX6675_MID_CS_PIN) |
                        (1ULL << MAX6675_BOT_CS_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t ret = gpio_config(&out_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure output GPIOs: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Configure SO (MISO) as input with weak pull-up */
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << MAX6675_SO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ret = gpio_config(&in_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure SO GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set default idle levels: SCK=0, all CS=1 (de-asserted) */
    gpio_set_level(MAX6675_SCK_PIN, 0);
    gpio_set_level(MAX6675_TOP_CS_PIN, 1);
    gpio_set_level(MAX6675_MID_CS_PIN, 1);
    gpio_set_level(MAX6675_BOT_CS_PIN, 1);

    ESP_LOGI(TAG, "MAX6675 driver initialized successfully");
    return ESP_OK;
}

static uint16_t max6675_read_raw_internal(gpio_num_t cs_pin)
{
    /* 1. Ensure all other CS lines are explicitly held HIGH */
    if (cs_pin != MAX6675_TOP_CS_PIN) gpio_set_level(MAX6675_TOP_CS_PIN, 1);
    if (cs_pin != MAX6675_MID_CS_PIN) gpio_set_level(MAX6675_MID_CS_PIN, 1);
    if (cs_pin != MAX6675_BOT_CS_PIN) gpio_set_level(MAX6675_BOT_CS_PIN, 1);

    /* 2. SCK must be strictly LOW before asserting CS */
    gpio_set_level(MAX6675_SCK_PIN, 0);
    esp_rom_delay_us(10);

    /* 3. Assert target CS (Active LOW) */
    gpio_set_level(cs_pin, 0);
    esp_rom_delay_us(25); /* t_CSS setup time */

    /* 4. Shift in 16 bits (MSB D15 first)
     * MAX6675 outputs D15 immediately on CS falling edge.
     * On each falling edge of SCK, MAX6675 shifts to the next bit.
     * Read SO while SCK is HIGH at 50 kHz for clean noise immunity.
     */
    uint16_t raw = 0;
    for (int i = 0; i < 16; i++) {
        gpio_set_level(MAX6675_SCK_PIN, 1);
        esp_rom_delay_us(10);

        raw = (raw << 1) | (gpio_get_level(MAX6675_SO_PIN) & 0x01);

        gpio_set_level(MAX6675_SCK_PIN, 0);
        esp_rom_delay_us(10);
    }

    /* 5. De-assert CS (Active HIGH) to start next 220ms conversion */
    gpio_set_level(cs_pin, 1);
    esp_rom_delay_us(50); /* t_CSH inactive settling time */

    return raw;
}

static max6675_reading_t max6675_read_channel_once(gpio_num_t cs_pin)
{
    max6675_reading_t reading = {
        .temperature_c = 0.0f,
        .valid = false,
        .is_open = false,
        .raw_value = 0
    };

    uint16_t raw = max6675_read_raw_internal(cs_pin);
    reading.raw_value = raw;

    if (raw == 0xFFFF || raw == 0x0000) {
        /* Bus floating (0xFFFF) or shorted to GND (0x0000) */
        reading.is_open = true;
        reading.valid = false;
        reading.temperature_c = 0.0f;
        return reading;
    }

    if (raw & 0x0004) {
        /* Confirmed open circuit on thermocouple input */
        reading.is_open = true;
        reading.valid = false;
        reading.temperature_c = 0.0f;
        return reading;
    }

    /* Extract 12-bit temperature (Bits 14..3, 0.25°C per LSB) */
    uint16_t temp_val = (raw >> 3) & 0x0FFF;
    reading.temperature_c = (float)temp_val * 0.25f;
    reading.is_open = false;

    /* Range check: Valid thermocouple temperature is > 0.0°C and <= 1024.0°C */
    if (temp_val > 0 && reading.temperature_c <= 1024.0f) {
        reading.valid = true;
    } else {
        reading.valid = false;
    }

    return reading;
}

max6675_reading_t max6675_read_channel(gpio_num_t cs_pin)
{
    max6675_reading_t reading = {0};
    for (int retry = 0; retry < 3; retry++) {
        reading = max6675_read_channel_once(cs_pin);
        if (reading.valid) {
            return reading;
        }
        /* 2ms settling delay between retries */
        esp_rom_delay_us(2000);
    }
    return reading;
}

static void process_channel_reading(max6675_reading_t *reading, channel_state_t *state, float *out_c, bool *out_valid, bool *out_open)
{
    if (reading->valid) {
        if (state->has_prev) {
            /* Outlier jump rejection (>60°C spike) */
            if (fabsf(reading->temperature_c - state->last_c) > 60.0f && state->rejection_count < 3) {
                *out_c = state->last_c;
                state->rejection_count++;
            } else {
                /* Apply Exponential Moving Average (EMA) smoothing: 70% history, 30% new */
                float smoothed = (0.7f * state->last_c) + (0.3f * reading->temperature_c);
                state->last_c = smoothed;
                *out_c = smoothed;
                state->rejection_count = 0;
            }
        } else {
            state->last_c = reading->temperature_c;
            state->has_prev = true;
            *out_c = reading->temperature_c;
            state->rejection_count = 0;
        }
        *out_valid = true;
        *out_open = false;
    } else {
        /* Hold previous valid value for up to 3 consecutive failed samples */
        if (state->has_prev && state->rejection_count < 3) {
            state->rejection_count++;
            *out_c = state->last_c;
            *out_valid = true;
            *out_open = false;
        } else {
            *out_c = 0.0f;
            *out_valid = false;
            *out_open = reading->is_open;
            state->has_prev = false;
            state->rejection_count = 0;
        }
    }
}

temperature_sample_t max6675_sample_all(void)
{
    temperature_sample_t sample = {0};
    sample.timestamp_us = esp_timer_get_time();

#if MAX6675_ENABLE_TOP
    max6675_reading_t top = max6675_read_channel(MAX6675_TOP_CS_PIN);
    process_channel_reading(&top, &s_top_state, &sample.top_c, &sample.top_valid, &sample.top_open);
#else
    max6675_reading_t top = {0};
    sample.top_c = 0.0f;
    sample.top_valid = false;
    sample.top_open = true;
#endif

    /* 20 ms bus settling time between channels */
    esp_rom_delay_us(20000);

#if MAX6675_ENABLE_MID
    max6675_reading_t mid = max6675_read_channel(MAX6675_MID_CS_PIN);
    process_channel_reading(&mid, &s_mid_state, &sample.middle_c, &sample.middle_valid, &sample.middle_open);
#else
    max6675_reading_t mid = {0};
    sample.middle_c = 0.0f;
    sample.middle_valid = false;
    sample.middle_open = true;
#endif

    /* 20 ms bus settling time between channels */
    esp_rom_delay_us(20000);

#if MAX6675_ENABLE_BOT
    max6675_reading_t bot = max6675_read_channel(MAX6675_BOT_CS_PIN);
    process_channel_reading(&bot, &s_bot_state, &sample.bottom_c, &sample.bottom_valid, &sample.bottom_open);
#else
    max6675_reading_t bot = {0};
    sample.bottom_c = 0.0f;
    sample.bottom_valid = false;
    sample.bottom_open = true;
#endif

    ESP_LOGI(TAG, "RAW SPI READINGS -> TOP(CS7): 0x%04X (valid=%d), MID(CS15): 0x%04X (valid=%d), BOT(CS16): 0x%04X (valid=%d)",
             top.raw_value, sample.top_valid,
             mid.raw_value, sample.middle_valid,
             bot.raw_value, sample.bottom_valid);

    /* Calculate rates of change (dT/dt in °C/s) */
    if (s_last_timestamp_us > 0) {
        float dt_s = (float)(sample.timestamp_us - s_last_timestamp_us) / 1000000.0f;
        
        /* Cap dt_s to prevent extreme rate spikes if task gets starved */
        if (dt_s > 5.0f) {
            dt_s = 5.0f; 
        }

        if (dt_s > 0.05f) {
            if (sample.top_valid && s_top_state.has_prev) {
                float diff = sample.top_c - s_top_state.last_c;
                sample.top_rate = (fabsf(diff) > 30.0f) ? 0.0f : (diff / dt_s);
            }
            if (sample.middle_valid && s_mid_state.has_prev) {
                float diff = sample.middle_c - s_mid_state.last_c;
                sample.middle_rate = (fabsf(diff) > 30.0f) ? 0.0f : (diff / dt_s);
            }
            if (sample.bottom_valid && s_bot_state.has_prev) {
                float diff = sample.bottom_c - s_bot_state.last_c;
                sample.bottom_rate = (fabsf(diff) > 30.0f) ? 0.0f : (diff / dt_s);
            }
        }
    }

    s_last_timestamp_us = sample.timestamp_us;

    return sample;
}
