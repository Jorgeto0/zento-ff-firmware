#include "current_limit.h"
#include "coil_pwm.h"
#include "current_sense.h"
#include "coil_selftest.h"
#include "pico/time.h"

static uint16_t limit_ma[DRV_COUNT] = { 500, 500, 500, 500, 0 };
static uint32_t last_run_us = 0;

// Never scale below 1/16, so a coil can always recover if the reading
// glitches high. Step back up ~1.6% per period when under the limit.
#define SCALE_MIN   (COIL_SCALE_ONE / 16u)
#define SCALE_STEP  1024u

void current_limit_set_ma(drv_id_t id, uint16_t ma) {
    if (id >= DRV_COUNT) return;
    limit_ma[id] = ma;
    if (ma == 0) coil_set_scale_q16(id, COIL_SCALE_ONE);   // limit off
}

uint16_t current_limit_get_ma(drv_id_t id) {
    return (id < DRV_COUNT) ? limit_ma[id] : 0;
}

void current_limit_task(void) {
    if (selftest_running()) return;     // test measures without limits
    uint32_t now = time_us_32();
    if ((now - last_run_us) < CURRENT_LIMIT_PERIOD_US) return;
    last_run_us = now;

    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        drv_id_t id = (drv_id_t)i;
        uint16_t lim = limit_ma[i];
        if (lim == 0 || coil_get_cmd_level(id) == 0) continue;

        uint32_t s  = coil_get_scale_q16(id);
        uint16_t ma = current_coil_ma(id);

        if (ma > lim) {
            // Over: cut in proportion (current follows duty in PH/EN).
            s = (uint32_t)(((uint64_t)s * lim) / ma);
        } else if ((uint32_t)ma * 100u < (uint32_t)lim * 95u) {
            // Clearly under: creep back towards the requested level.
            s += SCALE_STEP;
        }
        if (s < SCALE_MIN)      s = SCALE_MIN;
        if (s > COIL_SCALE_ONE) s = COIL_SCALE_ONE;
        coil_set_scale_q16(id, s);
    }
}
