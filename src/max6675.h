#ifndef MAX6675_H
#define MAX6675_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature_c;
    bool valid;
    bool is_open;
    uint16_t raw_value;
} max6675_reading_t;

typedef struct {
    int64_t timestamp_us;

    float top_c;
    float middle_c;
    float bottom_c;

    bool top_valid;
    bool middle_valid;
    bool bottom_valid;

    bool top_open;
    bool middle_open;
    bool bottom_open;

    float top_rate;
    float middle_rate;
    float bottom_rate;
} temperature_sample_t;

/**
 * @brief Initialize GPIOs for 3x MAX6675 modules (shared SCK/SO, individual CS).
 * @return ESP_OK on success.
 */
esp_err_t max6675_init(void);

/**
 * @brief Sequentially read a single MAX6675 channel via its CS pin.
 * @param cs_pin The GPIO number of the specific chip select line.
 * @return max6675_reading_t with temperature in °C and validity/open status.
 */
max6675_reading_t max6675_read_channel(gpio_num_t cs_pin);

/**
 * @brief Sequentially read TOP, MIDDLE, and BOTTOM MAX6675 sensors,
 * compute rate of change relative to previous reading, and return complete sample.
 * @return temperature_sample_t
 */
temperature_sample_t max6675_sample_all(void);

#ifdef __cplusplus
}
#endif

#endif /* MAX6675_H */
