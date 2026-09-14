#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "data_model.h"
#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the NimBLE Bluetooth Low Energy peripheral subsystem.
 * Advertises as 'KriSHE-Carbon' with Service UUID 0xFFE0 and Characteristic 0xFFE1.
 * @return ESP_OK on success.
 */
esp_err_t ble_service_init(void);

/**
 * @brief Push the latest telemetry snapshot to connected BLE client(s) via notification.
 * @param st Pointer to current snapshot.
 */
void ble_service_notify_state(const device_state_t *st);

/**
 * @brief Check if a central client is currently connected to BLE.
 * @return true if connected.
 */
bool ble_service_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_SERVICE_H */
