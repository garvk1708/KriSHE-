#ifndef KILN_STATE_H
#define KILN_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "max6675.h"
#include "gnss.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KILN_STATE_IDLE = 0,
    KILN_STATE_RESTING,
    KILN_STATE_PREHEATING,
    KILN_STATE_ACTIVE,
    KILN_STATE_COOLDOWN,
    KILN_STATE_COMPLETE
} kiln_state_t;

typedef struct {
    char batch_id[32];
    bool is_active;
    int64_t start_time_us;
    int64_t start_utc_epoch;
    double start_latitude;
    double start_longitude;
    float start_top_c;
    float start_mid_c;
    float start_bot_c;
    uint32_t duration_s;
    kiln_state_t final_state;
} kiln_session_t;

/**
 * @brief Convert kiln_state_t enum to string representation.
 */
const char *kiln_state_to_str(kiln_state_t state);

/**
 * @brief Initialize the kiln state machine.
 */
void kiln_state_init(void);

/**
 * @brief Execute one iteration of the kiln state machine with real sensor sample and GNSS data.
 * @param sample Real thermocouple sample.
 * @param gnss Real GNSS fix.
 * @return Current updated kiln state.
 */
kiln_state_t kiln_state_process(const temperature_sample_t *sample, const gnss_fix_t *gnss);

/**
 * @brief Retrieve current kiln session data.
 */
kiln_session_t kiln_state_get_session(void);

/**
 * @brief Retrieve current kiln state.
 */
kiln_state_t kiln_state_get_current(void);

#ifdef __cplusplus
}
#endif

#endif /* KILN_STATE_H */
