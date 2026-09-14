#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * DEVICE INFORMATION
 * ========================================================================= */
#define DEVICE_NAME             "KriSHE Carbon Sensor Node"
#define FIRMWARE_VERSION        "1.0.0"

/* =========================================================================
 * FINAL HARDWARE PINOUT — DO NOT CHANGE
 * ========================================================================= */
#define MAX6675_SCK_PIN         GPIO_NUM_5   /* Shared SPI Serial Clock */
#define MAX6675_SO_PIN          GPIO_NUM_6   /* Shared SPI Serial Output (MISO) */
#define MAX6675_TOP_CS_PIN      GPIO_NUM_7   /* Top Thermocouple Chip Select */
#define MAX6675_MID_CS_PIN      GPIO_NUM_15  /* Middle Thermocouple Chip Select */
#define MAX6675_BOT_CS_PIN      GPIO_NUM_16  /* Bottom Thermocouple Chip Select */

/* Channel Hardware Enable Switches (Set to 0 if a probe is physically disconnected/broken) */
#define MAX6675_ENABLE_TOP      1
#define MAX6675_ENABLE_MID      1
#define MAX6675_ENABLE_BOT      1

/* L89HA GNSS UART */
#define GNSS_UART_NUM           UART_NUM_2
#define GNSS_UART_RX_PIN        GPIO_NUM_17  /* ESP32 RX <- L89HA TX */
#define GNSS_UART_TX_PIN        GPIO_NUM_18  /* ESP32 TX -> L89HA RX */
#define GNSS_UART_BAUD_RATE     9600         /* L89HA default baud */
#define GNSS_UART_BUF_SIZE      2048

/* =========================================================================
 * SAMPLING & TIMING CONFIGURATION
 * ========================================================================= */
#define TEMP_SAMPLE_PERIOD_MS   1000         /* 1 Hz sampling frequency */

/* =========================================================================
 * KILN STATE MACHINE CONFIGURATION
 * ========================================================================= */
#define ACTIVATION_THRESHOLD_C  60.0f        /* Kiln activation threshold */
#define PREHEAT_THRESHOLD_C     35.0f        /* Rise detection threshold */
#define COOLDOWN_THRESHOLD_C    55.0f        /* Fall threshold for cooldown */
#define COOLDOWN_SAMPLES_REQ    5            /* Consecutive low samples required */
#define RESTING_TIMEOUT_S       30           /* Continuous no-heat duration before switching to RESTING state */

typedef enum {
    ZONE_CRITERION_ANY = 0,     /* Any valid zone above threshold */
    ZONE_CRITERION_MAJORITY,    /* Majority of valid zones above threshold (DEFAULT) */
    ZONE_CRITERION_ALL          /* All valid zones above threshold */
} zone_criterion_t;

#define ACTIVE_ZONE_CRITERION   ZONE_CRITERION_MAJORITY

/* =========================================================================
 * WI-FI ACCESS POINT CONFIGURATION
 * ========================================================================= */
#define WIFI_AP_SSID            "KriSHE_Carbon_AP"
#define WIFI_AP_PASS            "krishecarbon"
#define WIFI_AP_CHANNEL         1
#define WIFI_AP_MAX_CONN        4

/* =========================================================================
 * FREERTOS TASK PRIORITIES & STACK SIZES
 * ========================================================================= */
#define SENSOR_TASK_STACK_SIZE  4096
#define SENSOR_TASK_PRIORITY    5

#define GNSS_TASK_STACK_SIZE    4096
#define GNSS_TASK_PRIORITY      4

#define STATE_TASK_STACK_SIZE   4096
#define STATE_TASK_PRIORITY     3

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */
