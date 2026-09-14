#include "kiln_state.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "KILN_STATE";

static kiln_state_t s_current_state = KILN_STATE_IDLE;
static kiln_session_t s_current_session = {0};
static int s_cooldown_counter = 0;
static int s_no_heat_counter = 0;

const char *kiln_state_to_str(kiln_state_t state)
{
    switch (state) {
        case KILN_STATE_IDLE:       return "IDLE";
        case KILN_STATE_RESTING:    return "RESTING";
        case KILN_STATE_PREHEATING: return "PREHEATING";
        case KILN_STATE_ACTIVE:     return "ACTIVE";
        case KILN_STATE_COOLDOWN:   return "COOLDOWN";
        case KILN_STATE_COMPLETE:   return "COMPLETE";
        default:                    return "UNKNOWN";
    }
}

void kiln_state_init(void)
{
    s_current_state = KILN_STATE_IDLE;
    memset(&s_current_session, 0, sizeof(s_current_session));
    s_cooldown_counter = 0;
    s_no_heat_counter = 0;
    ESP_LOGI(TAG, "Kiln state machine initialized in IDLE state");
}

static bool check_zone_threshold(const temperature_sample_t *sample, float threshold, zone_criterion_t criterion)
{
    int valid_count = 0;
    int over_count = 0;

    if (sample->top_valid) {
        valid_count++;
        if (sample->top_c >= threshold) over_count++;
    }
    if (sample->middle_valid) {
        valid_count++;
        if (sample->middle_c >= threshold) over_count++;
    }
    if (sample->bottom_valid) {
        valid_count++;
        if (sample->bottom_c >= threshold) over_count++;
    }

    if (valid_count == 0) {
        return false; /* No valid sensors available */
    }

    switch (criterion) {
        case ZONE_CRITERION_ANY:
            return (over_count >= 1);
        case ZONE_CRITERION_ALL:
            return (over_count == valid_count);
        case ZONE_CRITERION_MAJORITY:
        default:
            return (over_count > (valid_count / 2));
    }
}

kiln_state_t kiln_state_process(const temperature_sample_t *sample, const gnss_fix_t *gnss)
{
    if (!sample) return s_current_state;

    bool heat_detected = check_zone_threshold(sample, PREHEAT_THRESHOLD_C, ZONE_CRITERION_ANY);
    if (heat_detected) {
        s_no_heat_counter = 0;
    } else {
        s_no_heat_counter++;
    }

    switch (s_current_state) {
        case KILN_STATE_IDLE: {
            if (heat_detected) {
                s_current_state = KILN_STATE_PREHEATING;
                ESP_LOGI(TAG, "State transition: IDLE -> PREHEATING (heat detected)");
            } else if (s_no_heat_counter >= RESTING_TIMEOUT_S) {
                s_current_state = KILN_STATE_RESTING;
                ESP_LOGI(TAG, "State transition: IDLE -> RESTING (no heat applied for %d s)", RESTING_TIMEOUT_S);
            }
            break;
        }

        case KILN_STATE_RESTING: {
            /* Wake up immediately once heat is detected on any probe */
            if (heat_detected) {
                s_current_state = KILN_STATE_PREHEATING;
                ESP_LOGI(TAG, "State transition: RESTING -> PREHEATING (heat applied)");
            }
            break;
        }

        case KILN_STATE_PREHEATING: {
            /* Check if multi-zone activation criterion is met */
            if (check_zone_threshold(sample, ACTIVATION_THRESHOLD_C, ACTIVE_ZONE_CRITERION)) {
                s_current_state = KILN_STATE_ACTIVE;
                s_cooldown_counter = 0;

                /* Initialize logical session */
                s_current_session.is_active = true;
                s_current_session.start_time_us = esp_timer_get_time();
                s_current_session.start_utc_epoch = gnss ? gnss->utc_epoch : 0;
                s_current_session.start_latitude = gnss ? gnss->latitude : 0.0;
                s_current_session.start_longitude = gnss ? gnss->longitude : 0.0;
                s_current_session.start_top_c = sample->top_c;
                s_current_session.start_mid_c = sample->middle_c;
                s_current_session.start_bot_c = sample->bottom_c;
                s_current_session.duration_s = 0;

                /* Generate Unique Batch ID */
                uint32_t rand_id = esp_random() & 0xFFFF;
                snprintf(s_current_session.batch_id, sizeof(s_current_session.batch_id),
                         "KILN-%04lX-%04lX", (long)(s_current_session.start_time_us / 1000000) & 0xFFFF, (long)rand_id);

                ESP_LOGI(TAG, "State transition: PREHEATING -> ACTIVE. New Batch ID: %s", s_current_session.batch_id);
            } else if (!heat_detected && s_no_heat_counter >= 15) {
                /* Cooled back to ambient without reaching activation */
                s_current_state = KILN_STATE_RESTING;
                ESP_LOGI(TAG, "State transition: PREHEATING -> RESTING (no heat applied)");
            }
            break;
        }

        case KILN_STATE_ACTIVE: {
            s_current_session.duration_s = (uint32_t)((esp_timer_get_time() - s_current_session.start_time_us) / 1000000);

            /* Check if temperatures fall below cooldown threshold */
            if (!check_zone_threshold(sample, COOLDOWN_THRESHOLD_C, ACTIVE_ZONE_CRITERION)) {
                s_current_state = KILN_STATE_COOLDOWN;
                s_cooldown_counter = 0;
                ESP_LOGI(TAG, "State transition: ACTIVE -> COOLDOWN");
            }
            break;
        }

        case KILN_STATE_COOLDOWN: {
            s_current_session.duration_s = (uint32_t)((esp_timer_get_time() - s_current_session.start_time_us) / 1000000);

            /* If temperature flares back up, return to ACTIVE */
            if (check_zone_threshold(sample, COOLDOWN_THRESHOLD_C, ACTIVE_ZONE_CRITERION)) {
                s_current_state = KILN_STATE_ACTIVE;
                s_cooldown_counter = 0;
                ESP_LOGI(TAG, "State transition: COOLDOWN -> ACTIVE (reheat detected)");
            } else {
                s_cooldown_counter++;
                ESP_LOGD(TAG, "Cooldown sample %d/%d", s_cooldown_counter, COOLDOWN_SAMPLES_REQ);
                if (s_cooldown_counter >= COOLDOWN_SAMPLES_REQ) {
                    s_current_state = KILN_STATE_COMPLETE;
                    s_current_session.final_state = KILN_STATE_COMPLETE;
                    ESP_LOGI(TAG, "State transition: COOLDOWN -> COMPLETE. Batch %s finished. Total duration: %lu s",
                             s_current_session.batch_id, (unsigned long)s_current_session.duration_s);
                }
            }
            break;
        }

        case KILN_STATE_COMPLETE: {
            /* Session finalized; transition back to RESTING */
            s_current_session.is_active = false;
            s_current_state = KILN_STATE_RESTING;
            ESP_LOGI(TAG, "State transition: COMPLETE -> RESTING. Ready for next session.");
            break;
        }
    }

    return s_current_state;
}

kiln_session_t kiln_state_get_session(void)
{
    return s_current_session;
}

kiln_state_t kiln_state_get_current(void)
{
    return s_current_state;
}
