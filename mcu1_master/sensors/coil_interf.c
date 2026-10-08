#include "coil_interf.h"
#include "tmag5170.h"
#include "as5047p.h"
#include "motor/coil_pwm.h"
#include "pico/time.h"

#define SETTLE_MS     150u    // all off before the baseline
#define WINDOW_MS     150u    // averaging window
#define DRIVE_MS      300u    // per coil: settle WINDOW_MS, then average
#define REST_MS       150u    // off between coils
#define SAMPLE_MS     5u      // sensor read interval inside a window

typedef enum { IF_IDLE, IF_SETTLE, IF_BASE, IF_DRIVE, IF_REST } if_state_t;

static if_state_t       state = IF_IDLE;
static uint8_t          coil  = 0;
static uint32_t         t0 = 0, last_sample = 0;
static interf_result_t  res[DRV_COUNT];
static interf_base_t    base;

// Running sums for the current window
static int32_t  sx, sy, sz, sang;
static uint16_t nt, na;
static uint16_t ang_ref;          // angle the window's diffs are taken from

static uint32_t now_ms(void) { return to_ms_since_boot(get_absolute_time()); }

// Signed difference of two 14-bit angles, wrapped to -8192..8191
static int32_t ang_diff(uint16_t a, uint16_t b) {
    int32_t d = (int32_t)(a & 0x3FFF) - (int32_t)(b & 0x3FFF);
    if (d >  8191) d -= 16384;
    if (d < -8192) d += 16384;
    return d;
}

static void window_reset(void) { sx = sy = sz = sang = 0; nt = na = 0; }

static void sample(void) {
    if (now_ms() - last_sample < SAMPLE_MS) return;
    last_sample = now_ms();
    tmag_xyz_t m;
    if (tmag_read_xyz(&m) == TMAG_OK) {
        sx += m.x; sy += m.y; sz += m.z; nt++;
    }
    uint16_t a;
    if (as5047_read_angle(&a) == AS_OK) {
        sang += ang_diff(a, ang_ref); na++;
    }
}

void interf_start(void) {
    coil_all_off();
    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        res[i] = (interf_result_t){0};
        coil_set_scale_q16((drv_id_t)i, COIL_SCALE_ONE);   // no amp limit
    }
    base = (interf_base_t){0};
    uint16_t a;
    ang_ref = (as5047_read_angle(&a) == AS_OK) ? a : 0;
    coil = 0;
    t0 = now_ms();
    state = IF_SETTLE;
}

bool interf_running(void) { return state != IF_IDLE; }
const interf_result_t *interf_results(void) { return res; }
const interf_base_t   *interf_baseline(void) { return &base; }

static int16_t clamp16(int32_t v) {
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

bool interf_task(void) {
    uint32_t el = now_ms() - t0;

    switch (state) {
    case IF_SETTLE:
        if (el < SETTLE_MS) return false;
        window_reset();
        t0 = now_ms();
        state = IF_BASE;
        return false;

    case IF_BASE:
        sample();
        if (el < WINDOW_MS) return false;
        if (nt) {
            base.x = (int16_t)(sx / nt); base.y = (int16_t)(sy / nt); base.z = (int16_t)(sz / nt);
            base.ok |= 1u;
        }
        if (na) {
            base.ang = (uint16_t)((ang_ref + (sang / na)) & 0x3FFF);
            base.ok |= 2u;
        }
        coil_set_force((drv_id_t)coil, 32767);
        t0 = now_ms();
        window_reset();
        state = IF_DRIVE;
        return false;

    case IF_DRIVE:
        if (el >= (DRIVE_MS - WINDOW_MS)) sample();   // last WINDOW_MS only
        if (el < DRIVE_MS) return false;
        if (nt && (base.ok & 1u)) {
            res[coil].dx = clamp16(sx / nt - base.x);
            res[coil].dy = clamp16(sy / nt - base.y);
            res[coil].dz = clamp16(sz / nt - base.z);
            res[coil].ok |= 1u;
        }
        if (na && (base.ok & 2u)) {
            int32_t avg = (int32_t)ang_ref + sang / na;
            res[coil].dang = (int16_t)ang_diff((uint16_t)(avg & 0x3FFF), base.ang);
            res[coil].ok |= 2u;
        }
        coil_set_force((drv_id_t)coil, 0);
        t0 = now_ms();
        state = IF_REST;
        return false;

    case IF_REST:
        if (el < REST_MS) return false;
        if (++coil < DRV_COUNT) {
            coil_set_force((drv_id_t)coil, 32767);
            t0 = now_ms();
            window_reset();
            state = IF_DRIVE;
            return false;
        }
        coil_all_off();
        state = IF_IDLE;
        return true;

    default:
        return false;
    }
}
