#include "gnss.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "GNSS";

static gnss_fix_t s_latest_fix = {
    .latitude = 0.0,
    .longitude = 0.0,
    .utc_epoch = 0,
    .utc_str = "UNAVAILABLE",
    .satellites = 0,
    .location_valid = false,
    .time_valid = false
};

static SemaphoreHandle_t s_gnss_mutex = NULL;
static char s_line_buf[160];
static size_t s_line_idx = 0;

static double convert_nmea_coord(const char *coord_str, char dir)
{
    if (!coord_str || strlen(coord_str) < 4) return 0.0;

    double raw = atof(coord_str);
    int degrees = (int)(raw / 100.0);
    double minutes = raw - (degrees * 100.0);
    double dec_deg = (double)degrees + (minutes / 60.0);

    if (dir == 'S' || dir == 'W') {
        dec_deg = -dec_deg;
    }
    return dec_deg;
}

static int64_t s_last_valid_fix_time_us = 0;

static void parse_rmc(char *sentence)
{
    /* Format: $--RMC,time,status,lat,NS,lon,EW,spd,cog,date,mv,mvE,mode*cs */
    char *tokens[16];
    int token_count = 0;
    char *p = sentence;

    tokens[token_count++] = p;
    while (*p && token_count < 16) {
        if (*p == ',') {
            *p = '\0';
            tokens[token_count++] = p + 1;
        } else if (*p == '*') {
            *p = '\0';
            break;
        }
        p++;
    }

    if (token_count < 10) return;

    const char *time_str = tokens[1];
    const char *status   = tokens[2];
    const char *lat_str  = tokens[3];
    const char *lat_dir  = tokens[4];
    const char *lon_str  = tokens[5];
    const char *lon_dir  = tokens[6];
    const char *date_str = tokens[9];

    if (xSemaphoreTake(s_gnss_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        /* Fix validity: 'A' = Valid active lock, 'V' = Void / Searching */
        if (*status == 'A' && strlen(lat_str) > 0 && strlen(lon_str) > 0) {
            s_latest_fix.location_valid = true;
            s_latest_fix.latitude = convert_nmea_coord(lat_str, lat_dir[0]);
            s_latest_fix.longitude = convert_nmea_coord(lon_str, lon_dir[0]);
            s_last_valid_fix_time_us = esp_timer_get_time();

            /* Parse UTC Time & Date */
            if (strlen(time_str) >= 6 && strlen(date_str) == 6) {
                int hour = (time_str[0] - '0') * 10 + (time_str[1] - '0');
                int min  = (time_str[2] - '0') * 10 + (time_str[3] - '0');
                int sec  = (time_str[4] - '0') * 10 + (time_str[5] - '0');

                int day  = (date_str[0] - '0') * 10 + (date_str[1] - '0');
                int mon  = (date_str[2] - '0') * 10 + (date_str[3] - '0');
                int yr   = (date_str[4] - '0') * 10 + (date_str[5] - '0') + 2000;

                if (day >= 1 && day <= 31 && mon >= 1 && mon <= 12 && hour < 24 && min < 60 && sec < 60) {
                    struct tm t = {0};
                    t.tm_year = yr - 1900;
                    t.tm_mon  = mon - 1;
                    t.tm_mday = day;
                    t.tm_hour = hour;
                    t.tm_min  = min;
                    t.tm_sec  = sec;

                    s_latest_fix.utc_epoch = mktime(&t);
                    snprintf(s_latest_fix.utc_str, sizeof(s_latest_fix.utc_str),
                             "%04u-%02u-%02u %02u:%02u:%02u",
                             (unsigned int)yr, (unsigned int)mon, (unsigned int)day,
                             (unsigned int)hour, (unsigned int)min, (unsigned int)sec);
                    s_latest_fix.time_valid = true;
                }
            }
        } else if (*status == 'V') {
            /* If no valid fix from RMC or GGA in past 15 seconds, mark invalid */
            int64_t now_us = esp_timer_get_time();
            if (s_last_valid_fix_time_us == 0 || (now_us - s_last_valid_fix_time_us) > 15000000LL) {
                s_latest_fix.location_valid = false;
                s_latest_fix.latitude = 0.0;
                s_latest_fix.longitude = 0.0;
                s_latest_fix.time_valid = false;
                strncpy(s_latest_fix.utc_str, "SEARCHING / NO FIX", sizeof(s_latest_fix.utc_str) - 1);
            }
        }
        xSemaphoreGive(s_gnss_mutex);
    }
}

static void parse_gga(char *sentence)
{
    /* Format: $--GGA,time,lat,NS,lon,EW,quality,num_sat,hdop,alt,M,sep,M,age,ref*cs */
    char *tokens[16];
    int token_count = 0;
    char *p = sentence;

    tokens[token_count++] = p;
    while (*p && token_count < 16) {
        if (*p == ',') {
            *p = '\0';
            tokens[token_count++] = p + 1;
        } else if (*p == '*') {
            *p = '\0';
            break;
        }
        p++;
    }

    if (token_count < 8) return;

    const char *lat_str     = tokens[2];
    const char *lat_dir     = tokens[3];
    const char *lon_str     = tokens[4];
    const char *lon_dir     = tokens[5];
    const char *quality_str = tokens[6];
    const char *sat_str     = tokens[7];

    int quality = atoi(quality_str);
    int sats = atoi(sat_str);

    if (xSemaphoreTake(s_gnss_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_latest_fix.satellites = (uint8_t)(sats > 0 ? sats : 0);
        if (quality > 0 && strlen(lat_str) > 0 && strlen(lon_str) > 0) {
            s_latest_fix.location_valid = true;
            s_latest_fix.latitude = convert_nmea_coord(lat_str, lat_dir[0]);
            s_latest_fix.longitude = convert_nmea_coord(lon_str, lon_dir[0]);
            s_last_valid_fix_time_us = esp_timer_get_time();
        }
        xSemaphoreGive(s_gnss_mutex);
    }
}

static void parse_gsv(char *sentence)
{
    /* Format: $--GSV,total_msgs,msg_num,total_sats_in_view,...*cs */
    char *tokens[8];
    int token_count = 0;
    char *p = sentence;

    tokens[token_count++] = p;
    while (*p && token_count < 8) {
        if (*p == ',') {
            *p = '\0';
            tokens[token_count++] = p + 1;
        } else if (*p == '*') {
            *p = '\0';
            break;
        }
        p++;
    }

    if (token_count >= 4) {
        int sats_in_view = atoi(tokens[3]);
        if (sats_in_view > 0) {
            if (xSemaphoreTake(s_gnss_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (sats_in_view > s_latest_fix.satellites_in_view) {
                    s_latest_fix.satellites_in_view = (uint8_t)sats_in_view;
                }
                xSemaphoreGive(s_gnss_mutex);
            }
        }
    }
}

static void process_nmea_line(char *line)
{
    if (!line || strlen(line) < 6) return;

    if (xSemaphoreTake(s_gnss_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        s_latest_fix.sentences_received++;
        if (strstr(line, "RMC") || strstr(line, "GGA")) {
            strncpy(s_latest_fix.last_nmea, line, sizeof(s_latest_fix.last_nmea) - 1);
            s_latest_fix.last_nmea[sizeof(s_latest_fix.last_nmea) - 1] = '\0';
        }
        xSemaphoreGive(s_gnss_mutex);
    }

    /* Check for RMC (e.g. $GPRMC, $GNRMC) */
    if (strstr(line, "RMC")) {
        parse_rmc(line);
    }
    /* Check for GGA (e.g. $GPGGA, $GNGGA) */
    else if (strstr(line, "GGA")) {
        parse_gga(line);
    }
    /* Check for GSV (satellites in view) */
    else if (strstr(line, "GSV")) {
        parse_gsv(line);
    }
}

esp_err_t gnss_init(void)
{
    ESP_LOGI(TAG, "Initializing GNSS UART (Num: %d, RX: %d, TX: %d, Baud: %d)",
             GNSS_UART_NUM, GNSS_UART_RX_PIN, GNSS_UART_TX_PIN, GNSS_UART_BAUD_RATE);

    s_gnss_mutex = xSemaphoreCreateMutex();
    if (!s_gnss_mutex) {
        ESP_LOGE(TAG, "Failed to create GNSS mutex");
        return ESP_ERR_NO_MEM;
    }

    uart_config_t uart_config = {
        .baud_rate = GNSS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(GNSS_UART_NUM, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(GNSS_UART_NUM, GNSS_UART_TX_PIN, GNSS_UART_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_driver_install(GNSS_UART_NUM, GNSS_UART_BUF_SIZE, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "GNSS UART driver installed successfully");
    return ESP_OK;
}

void gnss_update(void)
{
    uint8_t buf[128];
    int rx_bytes = uart_read_bytes(GNSS_UART_NUM, buf, sizeof(buf), pdMS_TO_TICKS(100));
    if (rx_bytes > 0) {
        if (xSemaphoreTake(s_gnss_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            s_latest_fix.bytes_received += rx_bytes;
            xSemaphoreGive(s_gnss_mutex);
        }
        for (int i = 0; i < rx_bytes; i++) {
            char c = (char)buf[i];
            if (c == '$') {
                s_line_idx = 0;
                s_line_buf[s_line_idx++] = c;
            } else if (c == '\n' || c == '\r') {
                if (s_line_idx > 0) {
                    s_line_buf[s_line_idx] = '\0';
                    process_nmea_line(s_line_buf);
                    s_line_idx = 0;
                }
            } else if (s_line_idx < sizeof(s_line_buf) - 1) {
                s_line_buf[s_line_idx++] = c;
            }
        }
    }
}

gnss_fix_t gnss_get_fix(void)
{
    gnss_fix_t fix = {0};
    if (s_gnss_mutex && xSemaphoreTake(s_gnss_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        fix = s_latest_fix;
        xSemaphoreGive(s_gnss_mutex);
    }
    return fix;
}
