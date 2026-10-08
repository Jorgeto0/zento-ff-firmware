#ifndef RIGHT_SELFTEST_H
#define RIGHT_SELFTEST_H
// =============================================================================
// mcu1_master/motor/right_selftest.h — self-test of the five RIGHT coils
//
// Runs on MCU1 and drives the right coils through the PIO link, one coil at a
// time: rest, +50%, +100%, off, -100%, off. From MCU2's replies it records the
// current at each step, which coil's sense line actually saw the current, the
// state of the two MCU2 driver pins, the guard, link errors and MCU2's
// firmware version. The dashboard turns this into a verdict per coil.
// Pure logic, no hardware calls, so it is unit-tested on the PC.
// =============================================================================
#include <stdint.h>
#include <stdbool.h>

#define RST_COILS       5

// Pin class reported by MCU2 for one driver input (2 bits)
#define RST_PIN_LOW     0u   // low for the whole sample window
#define RST_PIN_TOGGLE  1u   // switching (PWM)
#define RST_PIN_HIGH    2u   // high for the whole sample window
#define RST_PIN_NODATA  3u   // no reply carried this coil's pin state

// Reply data handed in after each link exchange
typedef struct {
    uint16_t ma[RST_COILS];  // right coil currents, mA
    uint8_t  status;         // guard bits, bit per coil
    uint16_t sx;             // MCU2 fw version (low) | flags (high)
    uint16_t sy;             // probe echo (low, coil+1) | pin classes (high)
    uint16_t sz;             // MCU2 uptime, seconds
    uint16_t rx_err;         // MCU2's own receive error counter
} rst_reply_t;

enum { RST_LINK_OK = 0, RST_LINK_TIMEOUT, RST_LINK_CRC, RST_LINK_BAD };

typedef struct {
    uint16_t idle_ma;        // at rest, nothing commanded
    uint16_t f50_ma;         // +50%, average
    uint16_t f100_ma;        // +100%, average
    uint16_t f100_peak;      // +100%, highest reading (before any guard)
    uint16_t rev_ma;         // -100%, average
    uint16_t rev_peak;       // -100%, highest reading
    uint16_t pins;           // bits 0-3 rest, 4-7 +50%, 8-11 -100%;
                             // each nibble: PWM pin class | DIR class << 2
    uint8_t  guard;          // bit0 guard during +100%, bit1 during -100%
    uint8_t  other_idx;      // coil (1-5) whose sense saw the most current
                             // while this coil ran +100%, 0 = none
    uint16_t other_ma;       // that current
    uint8_t  samples;        // replies used for this coil
} rst_coil_t;

typedef struct {
    uint16_t link_ok, link_timeout, link_crc, link_bad;   // during the test
    uint8_t  slave_fw;       // MCU2 firmware version, 0 = before v30
    uint8_t  slave_flags;    // bit0 MCU2 last booted from its watchdog
    uint8_t  slave_reset;    // 1 = MCU2 restarted during the test
    uint8_t  slave_rx_err;   // MCU2 receive errors during the test (capped)
    rst_coil_t coil[RST_COILS];
} rst_result_t;

void rst_start(uint32_t now_ms);
bool rst_running(void);
// Forces and pin-probe request for the next exchange (probe: coil+1, 0 none)
void rst_fill(uint32_t now_ms, int16_t force[RST_COILS], uint8_t *probe);
// After each exchange. 'in' is only read when result == RST_LINK_OK.
void rst_on_reply(uint32_t now_ms, int result, const rst_reply_t *in);
// Advances the steps. Returns true once, when the test has just finished.
bool rst_task(uint32_t now_ms);
const rst_result_t *rst_results(void);
// Coil being tested (0-4) and step (0-5) for a progress display
uint8_t rst_progress(void);

#endif
