#include "data_model.h"
#include "config.h"
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "DATA_MODEL";

static device_state_t s_device_state;
static SemaphoreHandle_t s_data_mutex = NULL;

esp_err_t data_model_init(void)
{
    s_data_mutex = xSemaphoreCreateMutex();
    if (!s_data_mutex) {
        ESP_LOGE(TAG, "Failed to create data model mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(&s_device_state, 0, sizeof(s_device_state));
    strncpy(s_device_state.name, DEVICE_NAME, sizeof(s_device_state.name) - 1);
    strncpy(s_device_state.firmware, FIRMWARE_VERSION, sizeof(s_device_state.firmware) - 1);
    s_device_state.kiln_state = KILN_STATE_IDLE;
    strncpy(s_device_state.utc_str, "UNAVAILABLE", sizeof(s_device_state.utc_str) - 1);
    strncpy(s_device_state.batch_id, "NONE", sizeof(s_device_state.batch_id) - 1);

    ESP_LOGI(TAG, "Central data model initialized successfully");
    return ESP_OK;
}

void data_model_update_temp(const temperature_sample_t *sample)
{
    if (!sample || !s_data_mutex) return;

    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_device_state.top_c = sample->top_c;
        s_device_state.middle_c = sample->middle_c;
        s_device_state.bottom_c = sample->bottom_c;

        s_device_state.top_valid = sample->top_valid;
        s_device_state.middle_valid = sample->middle_valid;
        s_device_state.bottom_valid = sample->bottom_valid;

        s_device_state.top_open = sample->top_open;
        s_device_state.middle_open = sample->middle_open;
        s_device_state.bottom_open = sample->bottom_open;

        s_device_state.top_rate = sample->top_rate;
        s_device_state.middle_rate = sample->middle_rate;
        s_device_state.bottom_rate = sample->bottom_rate;

        s_device_state.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
        xSemaphoreGive(s_data_mutex);
    }
}

void data_model_update_gnss(const gnss_fix_t *fix)
{
    if (!fix || !s_data_mutex) return;

    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_device_state.latitude = fix->latitude;
        s_device_state.longitude = fix->longitude;
        s_device_state.location_valid = fix->location_valid;
        s_device_state.time_valid = fix->time_valid;
        s_device_state.satellites = fix->satellites;
        s_device_state.satellites_in_view = fix->satellites_in_view;
        s_device_state.gnss_rx_bytes = fix->bytes_received;
        s_device_state.gnss_sentences = fix->sentences_received;
        s_device_state.utc_epoch = fix->utc_epoch;
        strncpy(s_device_state.utc_str, fix->utc_str, sizeof(s_device_state.utc_str) - 1);
        strncpy(s_device_state.last_nmea, fix->last_nmea, sizeof(s_device_state.last_nmea) - 1);
        xSemaphoreGive(s_data_mutex);
    }
}

void data_model_update_kiln(kiln_state_t state, const kiln_session_t *session)
{
    if (!s_data_mutex) return;

    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_device_state.kiln_state = state;
        if (session) {
            strncpy(s_device_state.batch_id, session->batch_id[0] ? session->batch_id : "NONE",
                    sizeof(s_device_state.batch_id) - 1);
            s_device_state.session_active = session->is_active;
            s_device_state.session_duration_s = session->duration_s;
        }
        xSemaphoreGive(s_data_mutex);
    }
}

void data_model_update_system(bool wifi_active)
{
    if (!s_data_mutex) return;

    if (xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_device_state.wifi_active = wifi_active;
        s_device_state.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
        xSemaphoreGive(s_data_mutex);
    }
}

device_state_t data_model_get_snapshot(void)
{
    device_state_t snap = {0};
    if (s_data_mutex && xSemaphoreTake(s_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_device_state.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
        snap = s_device_state;
        xSemaphoreGive(s_data_mutex);
    }
    return snap;
}
