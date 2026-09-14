#ifndef GNSS_H
#define GNSS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double latitude;
    double longitude;

    int64_t utc_epoch;
    char utc_str[32];        /* Formatted: YYYY-MM-DD HH:MM:SS */

    uint8_t satellites;
    uint8_t satellites_in_view;

    uint32_t bytes_received;
    uint32_t sentences_received;
    char last_nmea[96];

    bool location_valid;
    bool time_valid;
} gnss_fix_t;

/**
 * @brief Initialize UART for the 7Semi L89HA GNSS module.
 * @return ESP_OK on success.
 */
esp_err_t gnss_init(void);

/**
 * @brief Read incoming bytes from GNSS UART and parse NMEA sentences.
 * Called continuously by the GNSS FreeRTOS task.
 */
void gnss_update(void);

/**
 * @brief Thread-safely retrieve the latest GNSS fix.
 * @return Current gnss_fix_t snapshot.
 */
gnss_fix_t gnss_get_fix(void);

#ifdef __cplusplus
}
#endif

#endif /* GNSS_H */
