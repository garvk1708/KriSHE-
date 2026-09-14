#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Wi-Fi Access Point (SSID: KriSHE_Carbon_AP).
 * @return ESP_OK on success.
 */
esp_err_t wifi_init_softap(void);

/**
 * @brief Start the ESP-IDF HTTP server exposing / and /api/status.
 * @return ESP_OK on success.
 */
esp_err_t start_web_server(void);

/**
 * @brief Stop the HTTP server.
 */
void stop_web_server(void);

/**
 * @brief Turn off Wi-Fi SoftAP and Web Server to save power & cool chip when BLE connects.
 */
void wifi_stop_softap(void);

/**
 * @brief Resume Wi-Fi SoftAP and Web Server when BLE disconnects.
 */
void wifi_resume_softap(void);

#ifdef __cplusplus
}
#endif

#endif /* DASHBOARD_H */
