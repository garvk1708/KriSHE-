#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "config.h"
#include "max6675.h"
#include "gnss.h"
#include "kiln_state.h"
#include "data_model.h"
#include "dashboard.h"

static const char *TAG = "MAIN";

static QueueHandle_t s_temp_queue = NULL;

/* -------------------------------------------------------------------------
 * TASK 1: Sensor Task (1 Hz sequential MAX6675 acquisition)
 * ------------------------------------------------------------------------- */
static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Sensor task started on core %d", xPortGetCoreID());
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1) {
        temperature_sample_t sample = max6675_sample_all();

        if (s_temp_queue) {
            /* Non-blocking push; if queue is full, overwrite oldest */
            if (xQueueSend(s_temp_queue, &sample, 0) != pdTRUE) {
                temperature_sample_t dummy;
                xQueueReceive(s_temp_queue, &dummy, 0);
                xQueueSend(s_temp_queue, &sample, 0);
            }
        }

        /* Relax sampling period during resting to minimize thermal dissipation */
        TickType_t period = (kiln_state_get_current() == KILN_STATE_RESTING)
                            ? pdMS_TO_TICKS(2000)
                            : pdMS_TO_TICKS(TEMP_SAMPLE_PERIOD_MS);
        vTaskDelayUntil(&last_wake_time, period);
    }
}

/* -------------------------------------------------------------------------
 * TASK 2: GNSS Task (Continuous NMEA streaming and parsing)
 * ------------------------------------------------------------------------- */
static void gnss_task(void *pvParameters)
{
    ESP_LOGI(TAG, "GNSS task started on core %d", xPortGetCoreID());

    while (1) {
        gnss_update();
        /* Brief sleep if no bytes to allow scheduler yielding */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

#include "ble_service.h"
#include "driver/temperature_sensor.h"

static temperature_sensor_handle_t s_internal_temp_sensor = NULL;

static void init_internal_temp_sensor(void)
{
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(20, 100);
    esp_err_t err = temperature_sensor_install(&temp_sensor_config, &s_internal_temp_sensor);
    if (err == ESP_OK) {
        err = temperature_sensor_enable(s_internal_temp_sensor);
        ESP_LOGI(TAG, "Internal chip die temperature sensor enabled successfully");
    } else {
        ESP_LOGW(TAG, "Failed to install internal temp sensor: %s", esp_err_to_name(err));
    }
}

static float get_internal_chip_temp(void)
{
    float tsens = 0.0f;
    if (s_internal_temp_sensor) {
        temperature_sensor_get_celsius(s_internal_temp_sensor, &tsens);
    }
    return tsens;
}

/* -------------------------------------------------------------------------
 * Helper: Print Periodic Formatted Serial Diagnostic Block (Every 3 seconds)
 * ------------------------------------------------------------------------- */
static void print_serial_diagnostics(const device_state_t *st)
{
    float die_temp = get_internal_chip_temp();
    printf("\n==================== [ KriSHE Carbon Kiln Monitor ] ====================\n");
    printf("  UPTIME: %-6lu s  |  STATE: %-12s  |  CHIP DIE: %4.1f °C\n",
           (unsigned long)st->uptime_s, kiln_state_to_str(st->kiln_state), die_temp);
    printf("------------------------------------------------------------------------\n");

    printf("  TEMPERATURES (1 Hz acquisition):\n");
    if (st->top_open) {
        printf("    TOP    (CS7):  [ PROBE OPEN / DISCONNECTED ]\n");
    } else if (!st->top_valid) {
        printf("    TOP    (CS7):  [ SENSOR FAULT ]\n");
    } else {
        printf("    TOP    (CS7):  %6.2f °C  (%+0.2f °C/s)\n", st->top_c, st->top_rate);
    }

    if (st->middle_open) {
        printf("    MIDDLE (CS15): [ PROBE OPEN / DISCONNECTED ]\n");
    } else if (!st->middle_valid) {
        printf("    MIDDLE (CS15): [ SENSOR FAULT ]\n");
    } else {
        printf("    MIDDLE (CS15): %6.2f °C  (%+0.2f °C/s)\n", st->middle_c, st->middle_rate);
    }

    if (st->bottom_open) {
        printf("    BOTTOM (CS16): [ PROBE OPEN / DISCONNECTED ]\n");
    } else if (!st->bottom_valid) {
        printf("    BOTTOM (CS16): [ SENSOR FAULT ]\n");
    } else {
        printf("    BOTTOM (CS16): %6.2f °C  (%+0.2f °C/s)\n", st->bottom_c, st->bottom_rate);
    }

    printf("------------------------------------------------------------------------\n");
    printf("  GNSS (7Semi L89HA UART2 @ 9600 baud):\n");
    if (st->gnss_rx_bytes == 0) {
        printf("    STATUS:     NO UART DATA (0 bytes recv)\n");
        printf("    DIAGNOSTIC: Check wiring -> L89HA TX to ESP32 GPIO17, RX to GPIO18\n");
    } else if (!st->location_valid) {
        printf("    STATUS:     SEARCHING (%u in view, %u used in fix) | %lu NMEA bytes\n",
               (unsigned int)st->satellites_in_view, (unsigned int)st->satellites,
               (unsigned long)st->gnss_rx_bytes);
        printf("    DIAGNOSTIC: GPS satellites require clear sky line-of-sight.\n");
        printf("                Indoor walls block 1.5 GHz signals. Move node near a window/outdoors.\n");
        printf("    UTC TIME:   %s\n", st->time_valid ? st->utc_str : "SEARCHING / NO CLOCK SYNC");
        printf("    RAW NMEA:   %s\n", st->last_nmea[0] ? st->last_nmea : "Waiting for NMEA frame...");
    } else {
        printf("    STATUS:     FIXED (3D Lock with %u sats, %u in view)\n",
               (unsigned int)st->satellites, (unsigned int)st->satellites_in_view);
        printf("    LATITUDE:   %.6f°\n", st->latitude);
        printf("    LONGITUDE:  %.6f°\n", st->longitude);
        printf("    UTC TIME:   %s\n", st->utc_str);
        printf("    RAW NMEA:   %s\n", st->last_nmea[0] ? st->last_nmea : "Fix active");
    }

    printf("------------------------------------------------------------------------\n");
    printf("  CONNECTIVITY:\n");
    printf("    BLE STATUS: %s\n", ble_service_is_connected() ?
           "CONNECTED to Companion App" : "ADVERTISING as 'KriSHE-Carbon' (Service 0xFFE0)");
    printf("    WI-FI AP:   SSID 'KriSHE_Carbon_AP' -> Web Dashboard http://192.168.4.1\n");
    printf("========================================================================\n\n");
    fflush(stdout);
}

/* -------------------------------------------------------------------------
 * TASK 3: State & Data Processing Task
 * ------------------------------------------------------------------------- */
static void state_task(void *pvParameters)
{
    ESP_LOGI(TAG, "State & Data processing task started on core %d", xPortGetCoreID());
    temperature_sample_t sample;
    int print_counter = 0;

    while (1) {
        /* Wait for new temperature sample from sensor task (1 Hz) */
        if (xQueueReceive(s_temp_queue, &sample, portMAX_DELAY) == pdTRUE) {
            /* 1. Get latest real GNSS fix */
            gnss_fix_t fix = gnss_get_fix();

            /* 2. Execute kiln state machine */
            kiln_state_t new_state = kiln_state_process(&sample, &fix);
            kiln_session_t session = kiln_state_get_session();

            /* 3. Update centralized thread-safe data model */
            data_model_update_temp(&sample);
            data_model_update_gnss(&fix);
            data_model_update_kiln(new_state, &session);

            /* 4. Stream real-time telemetry to BLE Central (Android App) at 1 Hz */
            device_state_t snap = data_model_get_snapshot();
            ble_service_notify_state(&snap);

            /* 5. Emit periodic serial diagnostic output every 3s for calm, stable viewing */
            print_counter++;
            if (print_counter >= 3) {
                print_counter = 0;
                print_serial_diagnostics(&snap);
            }
        }
    }
}

/* -------------------------------------------------------------------------
 * Application Entry Point
 * ------------------------------------------------------------------------- */
void app_main(void)
{
    ESP_LOGI(TAG, "Booting %s (FW: v%s)...", DEVICE_NAME, FIRMWARE_VERSION);

    /* Initialize Non-Volatile Storage (NVS) for Wi-Fi and Bluetooth */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 1. Initialize Central Data Model */
    ESP_ERROR_CHECK(data_model_init());

    /* 2. Initialize Hardware Drivers */
    init_internal_temp_sensor();
    ESP_ERROR_CHECK(max6675_init());
    ESP_ERROR_CHECK(gnss_init());
    kiln_state_init();

    /* 3. Initialize NimBLE Bluetooth Low Energy Peripheral */
    ESP_ERROR_CHECK(ble_service_init());

    /* 4. Create FreeRTOS Queue for temperature samples */
    s_temp_queue = xQueueCreate(4, sizeof(temperature_sample_t));
    if (!s_temp_queue) {
        ESP_LOGE(TAG, "Failed to create temperature queue");
        return;
    }

    /* 5. Start Wi-Fi SoftAP and Embedded Web Dashboard Server */
    ret = wifi_init_softap();
    if (ret == ESP_OK) {
        ret = start_web_server();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Web server failed to start, proceeding without dashboard");
        }
    } else {
        ESP_LOGW(TAG, "Wi-Fi SoftAP failed to start, proceeding in offline mode");
    }

    /* 6. Spawn Native FreeRTOS Tasks */
    xTaskCreatePinnedToCore(sensor_task, "sensor_task", SENSOR_TASK_STACK_SIZE, NULL,
                           SENSOR_TASK_PRIORITY, NULL, 1);

    xTaskCreatePinnedToCore(gnss_task, "gnss_task", GNSS_TASK_STACK_SIZE, NULL,
                           GNSS_TASK_PRIORITY, NULL, 1);

    xTaskCreatePinnedToCore(state_task, "state_task", STATE_TASK_STACK_SIZE, NULL,
                           STATE_TASK_PRIORITY, NULL, 0);

    ESP_LOGI(TAG, "System operational. Dual BLE & Wi-Fi active.");
}
