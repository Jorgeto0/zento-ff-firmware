#include "right_selftest.h"
#include <string.h>

// Steps per coil and how long each lasts. Readings are only taken in the
// second part of a step, once the coil current has settled (L/R of these
// coils is well under 5 ms) and MCU2's reply reflects the new command.
typedef enum { ST_REST = 0, ST_F50, ST_F100, ST_GAP1, ST_R100, ST_GAP2, ST_COUNT } step_t;
static const uint16_t step_ms[ST_COUNT]   = { 300, 400, 400, 300, 400, 300 };
static const uint16_t window_ms[ST_COUNT] = { 100, 200, 200, 0,   200, 0   };
static const int16_t  step_force[ST_COUNT] = { 0, 16384, 32767, 0, -32767, 0 };

static bool     running = false;
static uint8_t  coil = 0;
static step_t   step = ST_REST;
static uint32_t step_t0 = 0;
static rst_result_t res;

// Accumulators for the current step
static uint32_t acc_sum;
static uint16_t acc_n, acc_peak;

// MCU2 state tracking for the whole test
static bool     have_first;
static uint16_t first_rx_err, last_rx_err, last_up;

static void begin_step(uint32_t now_ms) {
    step_t0 = now_ms;
    acc_sum = 0; acc_n = 0; acc_peak = 0;
}

void rst_start(uint32_t now_ms) {
    memset(&res, 0, sizeof(res));
    for (uint8_t c = 0; c < RST_COILS; c++) {
        res.coil[c].pins = 0x0FFFu;   // every nibble "no data" until seen
    }
    have_first = false;
    first_rx_err = last_rx_err = last_up = 0;
    coil = 0;
    step = ST_REST;
    running = true;
    begin_step(now_ms);
}

bool rst_running(void) { return running; }

const rst_result_t *rst_results(void) { return &res; }

uint8_t rst_progress(void) { return (uint8_t)(coil * ST_COUNT + step); }

void rst_fill(uint32_t now_ms, int16_t force[RST_COILS], uint8_t *probe) {
    (void)now_ms;
    for (uint8_t i = 0; i < RST_COILS; i++) force[i] = 0;
    *probe = 0;
    if (!running) return;
    force[coil] = step_force[step];
    *probe = (uint8_t)(coil + 1u);
}

static void set_pins(rst_coil_t *c, uint8_t shift, uint8_t nib) {
    c->pins = (uint16_t)((c->pins & ~(0xFu << shift)) | ((uint16_t)nib << shift));
}

void rst_on_reply(uint32_t now_ms, int result, const rst_reply_t *in) {
    if (!running) return;
    switch (result) {
        case RST_LINK_OK:      res.link_ok++;      break;
        case RST_LINK_TIMEOUT: res.link_timeout++; return;
        case RST_LINK_CRC:     res.link_crc++;     return;
        default:               res.link_bad++;     return;
    }

    // MCU2 identity, restarts and its own error count
    res.slave_fw    = (uint8_t)(in->sx & 0xFF);
    res.slave_flags = (uint8_t)(in->sx >> 8);
    if (!have_first) {
        have_first = true;
        first_rx_err = in->rx_err;
    } else if (in->sz < last_up && (uint16_t)(last_up - in->sz) < 60000u) {
        res.slave_reset = 1;      // uptime went backwards: MCU2 rebooted
    }
    last_up = in->sz;
    last_rx_err = in->rx_err;
    uint16_t d = (uint16_t)(last_rx_err - first_rx_err);
    res.slave_rx_err = (uint8_t)(d > 255u ? 255u : d);

    rst_coil_t *c = &res.coil[coil];
    uint16_t ma = in->ma[coil];

    // Peak over the whole step: the right stick guard halves the power on
    // the first reading above 750 mA, so the full-power value only shows in
    // the first replies of the step.
    if (ma > acc_peak) acc_peak = ma;

    // Everything else only from the settled part of the step
    if (window_ms[step] == 0 || (now_ms - step_t0) < window_ms[step]) return;

    acc_sum += ma; acc_n++;
    if (c->samples < 255u) c->samples++;

    // Pin state, only if this reply is about the coil under test
    uint8_t echo = (uint8_t)(in->sy & 0xFF);
    uint8_t nib  = (uint8_t)((in->sy >> 8) & 0x0F);
    if (echo == coil + 1u) {
        if (step == ST_REST) set_pins(c, 0, nib);
        if (step == ST_F50)  set_pins(c, 4, nib);
        if (step == ST_R100) set_pins(c, 8, nib);
    }

    if (step == ST_F100) {
        if (in->status & (1u << coil)) c->guard |= 1u;
        // Which other sense line moved? Catches swapped channels or wiring.
        for (uint8_t o = 0; o < RST_COILS; o++) {
            if (o == coil) continue;
            if (in->ma[o] > c->other_ma) {
                c->other_ma  = in->ma[o];
                c->other_idx = (uint8_t)(o + 1u);
            }
        }
    }
    if (step == ST_R100 && (in->status & (1u << coil))) c->guard |= 2u;
}

static uint16_t avg(void) {
    return acc_n ? (uint16_t)((acc_sum + acc_n / 2u) / acc_n) : 0u;
}

bool rst_task(uint32_t now_ms) {
    if (!running) return false;
    if ((now_ms - step_t0) < step_ms[step]) return false;

    rst_coil_t *c = &res.coil[coil];
    switch (step) {
        case ST_REST: c->idle_ma = avg(); break;
        case ST_F50:  c->f50_ma  = avg(); break;
        case ST_F100: c->f100_ma = avg(); c->f100_peak = acc_peak; break;
        case ST_R100: c->rev_ma  = avg(); c->rev_peak  = acc_peak; break;
        default: break;
    }

    if (step + 1 < ST_COUNT) {
        step = (step_t)(step + 1);
    } else {
        step = ST_REST;
        if (++coil >= RST_COILS) {
            running = false;
            coil = 0;
            return true;
        }
    }
    begin_step(now_ms);
    return false;
}
