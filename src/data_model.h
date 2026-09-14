#ifndef DATA_MODEL_H
#define DATA_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "max6675.h"
#include "gnss.h"
#include "kiln_state.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* Device Info */
    char name[48];
    char firmware[16];
    uint32_t uptime_s;

    /* Temperature */
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

    /* Kiln Machine & Session */
    kiln_state_t kiln_state;
    char batch_id[32];
    bool session_active;
    uint32_t session_duration_s;

    /* GNSS Positioning */
    double latitude;
    double longitude;
    bool location_valid;
    bool time_valid;
    uint8_t satellites;
    uint8_t satellites_in_view;
    uint32_t gnss_rx_bytes;
    uint32_t gnss_sentences;
    int64_t utc_epoch;
    char utc_str[32];
    char last_nmea[96];

    /* System Status */
    bool wifi_active;
} device_state_t;

/**
 * @brief Initialize the central device state model and synchronizing primitives.
 * @return ESP_OK on success.
 */
esp_err_t data_model_init(void);

/**
 * @brief Update the temperature segment of the central device state.
 */
void data_model_update_temp(const temperature_sample_t *sample);

/**
 * @brief Update the GNSS segment of the central device state.
 */
void data_model_update_gnss(const gnss_fix_t *fix);

/**
 * @brief Update the kiln state and session segment of the central device state.
 */
void data_model_update_kiln(kiln_state_t state, const kiln_session_t *session);

/**
 * @brief Update system flags (e.g. Wi-Fi status, uptime).
 */
void data_model_update_system(bool wifi_active);

/**
 * @brief Retrieve a thread-safe snapshot of the complete device state.
 */
device_state_t data_model_get_snapshot(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_MODEL_H */
