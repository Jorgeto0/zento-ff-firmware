#ifndef COIL_SELFTEST_H
#define COIL_SELFTEST_H

// =============================================================================
// mcu1_master/motor/coil_selftest.h — one-click coil check
//
// Drives each coil in turn at 50%, 100%, then -100% (reverse), 250 ms each,
// and records two independent answers:
//   1. our current reading (IPROPI through the ADC)
//   2. the driver's own active open-load check (DIAG OL1/OL2)
// Plus the idle ADC reading with everything off, as a zero reference.
//
//   OL set,   ~0 A   -> nothing is conducting: coil path open
//   OL clear, ~0 A   -> coil conducts but our reading misses it
//   OL clear, amps   -> all good
//
// Runs as a state machine from the main loop, so USB and the watchdog keep
// running. ~4.5 s in total. Saved amp limits are ignored during the test so
// the 100% reading is a clean comparison against a bench measurement.
// =============================================================================

#include <stdbool.h>
#include <stdint.h>
#include "drv8873.h"

typedef struct {
    uint16_t idle_raw;   // ADC counts with every coil off
    uint16_t ma;         // current while driven at 50%
    uint16_t ma_full;    // current while driven at 100%
    uint16_t ma_rev;     // current while driven at -100% (reverse)
    uint8_t  fault;      // FAULT after driving, forward and reverse ORed
    uint8_t  diag;       // DIAG, forward and reverse ORed (OL1 bit 7, OL2 bit 6)
    uint8_t  ic1;        // IC1 as read during the test; MODE bits 1:0, 00 = PH/EN
    uint8_t  reconfig;   // times the driver had lost its config since boot
} selftest_result_t;

void selftest_start(void);
bool selftest_running(void);
bool selftest_task(void);                          // true once, when finished
const selftest_result_t *selftest_results(void);   // DRV_COUNT entries

#endif // COIL_SELFTEST_H
