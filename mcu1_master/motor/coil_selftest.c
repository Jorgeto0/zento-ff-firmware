#include "coil_selftest.h"
#include "coil_pwm.h"
#include "current_sense.h"
#include "pico/time.h"

#define SETTLE_MS   60u      // all off before measuring the zero reference
#define DRIVE_MS    250u     // per coil
#define REST_MS     60u      // off between coils
#define TEST_FORCE  16384    // 50% of full scale, gentle enough for every coil

typedef enum { ST_IDLE, ST_SETTLE, ST_DRIVE, ST_REST } st_state_t;

static st_state_t        state = ST_IDLE;
static uint8_t           coil  = 0;
static uint32_t          t0    = 0;
static selftest_result_t res[DRV_COUNT];

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

void selftest_start(void) {
    coil_all_off();
    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        res[i] = (selftest_result_t){0};
    }
    coil  = 0;
    t0    = now_ms();
    state = ST_SETTLE;
}

bool selftest_running(void) { return state != ST_IDLE; }

const selftest_result_t *selftest_results(void) { return res; }

static void begin_coil(void) {
    drv_clear_faults((drv_id_t)coil);      // fresh open-load verdict
    coil_set_force((drv_id_t)coil, TEST_FORCE);
    t0    = now_ms();
    state = ST_DRIVE;
}

bool selftest_task(void) {
    uint32_t el = now_ms() - t0;

    switch (state) {
    case ST_SETTLE:
        if (el < SETTLE_MS) return false;
        for (uint8_t i = 0; i < DRV_COUNT; i++) {
            res[i].idle_raw = current_raw((drv_id_t)i);
        }
        begin_coil();
        return false;

    case ST_DRIVE:
        if (el < DRIVE_MS) return false;
        res[coil].ma = current_coil_ma((drv_id_t)coil);
        drv_read_reg((drv_id_t)coil, DRV_REG_FAULT, &res[coil].fault, NULL);
        drv_read_reg((drv_id_t)coil, DRV_REG_DIAG,  &res[coil].diag,  NULL);
        coil_set_force((drv_id_t)coil, 0);
        t0    = now_ms();
        state = ST_REST;
        return false;

    case ST_REST:
        if (el < REST_MS) return false;
        if (++coil < DRV_COUNT) {
            begin_coil();
            return false;
        }
        coil_all_off();
        state = ST_IDLE;
        return true;

    default:
        return false;
    }
}
