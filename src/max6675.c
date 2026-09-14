#include "max6675.h"
#include "config.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include <math.h>

static const char *TAG = "MAX6675";

/* Tracking for rate of change calculation (dT/dt) */
static int64_t s_last_timestamp_us = 0;
static float s_last_top_c = 0.0f;
static float s_last_mid_c = 0.0f;
static float s_last_bot_c = 0.0f;
static bool s_has_prev_top = false;
static bool s_has_prev_mid = false;
static bool s_has_prev_bot = false;

/* Rejection counters to prevent permanent freezing */
static int s_top_rejection_count = 0;
static int s_mid_rejection_count = 0;
static int s_bot_rejection_count = 0;

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

max6675_reading_t max6675_read_channel(gpio_num_t cs_pin)
{
    max6675_reading_t reading = {
        .temperature_c = 0.0f,
        .valid = false,
        .is_open = false,
        .raw_value = 0
    };

    uint16_t raw = max6675_read_raw_internal(cs_pin);
    reading.raw_value = raw;

    /*
     * MAX6675 Bit Format:
     * D15: Dummy sign bit (ALWAYS 0)
     * D14..D3: 12-bit temperature (0.25°C / LSB)
     * D2:  Input Open indicator (1 = Open/Fault, 0 = Closed)
     * D1:  Device ID (0)
     * D0:  Three-state
     */

    if (raw == 0xFFFF || raw == 0x0000 || (raw & 0x8000)) {
        /* Bus floating (0xFFFF), shorted to GND (0x0000), or invalid D15 bit */
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

    /* Range check: MAX6675 range is 0.0°C to 1024.0°C */
    if (reading.temperature_c >= 0.0f && reading.temperature_c <= 1024.0f) {
        reading.valid = true;
    } else {
        reading.valid = false;
    }

    return reading;
}

temperature_sample_t max6675_sample_all(void)
{
    temperature_sample_t sample = {0};
    sample.timestamp_us = esp_timer_get_time();

    /* 1. Read TOP Channel */
#if MAX6675_ENABLE_TOP
    max6675_reading_t top = max6675_read_channel(MAX6675_TOP_CS_PIN);
    if (top.valid) {
        if (s_has_prev_top && fabsf(top.temperature_c - s_last_top_c) > 60.0f && s_top_rejection_count < 2) {
            /* Single outlier rejection: hold previous value for at most 2 samples */
            sample.top_c = s_last_top_c;
            sample.top_valid = true;
            sample.top_open = false;
            s_top_rejection_count++;
        } else {
            sample.top_c = top.temperature_c;
            sample.top_valid = true;
            sample.top_open = false;
            s_top_rejection_count = 0;
        }
    } else {
        sample.top_c = 0.0f;
        sample.top_valid = false;
        sample.top_open = top.is_open;
        s_top_rejection_count = 0;
    }
#else
    sample.top_c = 0.0f;
    sample.top_valid = false;
    sample.top_open = true;
#endif

    /* 20 ms bus settling time between channels */
    esp_rom_delay_us(20000);

    /* 2. Read MIDDLE Channel */
#if MAX6675_ENABLE_MID
    max6675_reading_t mid = max6675_read_channel(MAX6675_MID_CS_PIN);
    if (mid.valid) {
        if (s_has_prev_mid && fabsf(mid.temperature_c - s_last_mid_c) > 60.0f && s_mid_rejection_count < 2) {
            sample.middle_c = s_last_mid_c;
            sample.middle_valid = true;
            sample.middle_open = false;
            s_mid_rejection_count++;
        } else {
            sample.middle_c = mid.temperature_c;
            sample.middle_valid = true;
            sample.middle_open = false;
            s_mid_rejection_count = 0;
        }
    } else {
        sample.middle_c = 0.0f;
        sample.middle_valid = false;
        sample.middle_open = mid.is_open;
        s_mid_rejection_count = 0;
    }
#else
    sample.middle_c = 0.0f;
    sample.middle_valid = false;
    sample.middle_open = true;
#endif

    /* 20 ms bus settling time between channels */
    esp_rom_delay_us(20000);

    /* 3. Read BOTTOM Channel */
#if MAX6675_ENABLE_BOT
    max6675_reading_t bot = max6675_read_channel(MAX6675_BOT_CS_PIN);
    if (bot.valid) {
        if (s_has_prev_bot && fabsf(bot.temperature_c - s_last_bot_c) > 60.0f && s_bot_rejection_count < 2) {
            sample.bottom_c = s_last_bot_c;
            sample.bottom_valid = true;
            sample.bottom_open = false;
            s_bot_rejection_count++;
        } else {
            sample.bottom_c = bot.temperature_c;
            sample.bottom_valid = true;
            sample.bottom_open = false;
            s_bot_rejection_count = 0;
        }
    } else {
        sample.bottom_c = 0.0f;
        sample.bottom_valid = false;
        sample.bottom_open = bot.is_open;
        s_bot_rejection_count = 0;
    }
#else
    sample.bottom_c = 0.0f;
    sample.bottom_valid = false;
    sample.bottom_open = true;
#endif

    /* Calculate rates of change (dT/dt in °C/s) */
    if (s_last_timestamp_us > 0) {
        float dt_s = (float)(sample.timestamp_us - s_last_timestamp_us) / 1000000.0f;
        if (dt_s > 0.05f) {
            if (sample.top_valid && s_has_prev_top) {
                float diff = sample.top_c - s_last_top_c;
                sample.top_rate = (fabsf(diff) > 30.0f) ? 0.0f : (diff / dt_s);
            } else {
                sample.top_rate = 0.0f;
            }

            if (sample.middle_valid && s_has_prev_mid) {
                float diff = sample.middle_c - s_last_mid_c;
                sample.middle_rate = (fabsf(diff) > 30.0f) ? 0.0f : (diff / dt_s);
            } else {
                sample.middle_rate = 0.0f;
            }

            if (sample.bottom_valid && s_has_prev_bot) {
                float diff = sample.bottom_c - s_last_bot_c;
                sample.bottom_rate = (fabsf(diff) > 30.0f) ? 0.0f : (diff / dt_s);
            } else {
                sample.bottom_rate = 0.0f;
            }
        }
    }

    /* Update history for next iteration */
    s_last_timestamp_us = sample.timestamp_us;
    if (sample.top_valid) {
        s_last_top_c = sample.top_c;
        s_has_prev_top = true;
    } else {
        s_has_prev_top = false;
    }

    if (sample.middle_valid) {
        s_last_mid_c = sample.middle_c;
        s_has_prev_mid = true;
    } else {
        s_has_prev_mid = false;
    }

    if (sample.bottom_valid) {
        s_last_bot_c = sample.bottom_c;
        s_has_prev_bot = true;
    } else {
        s_has_prev_bot = false;
    }

    return sample;
}
