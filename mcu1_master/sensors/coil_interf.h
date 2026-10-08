#ifndef COIL_INTERF_H
#define COIL_INTERF_H

// =============================================================================
// mcu1_master/sensors/coil_interf.h — how much each coil disturbs the sensors
//
// All coils off: average the TMAG (X, Y, Z) and AS5047 angle as a baseline.
// Then each coil in turn at +100%: let it settle, average again, and report
// the change from the baseline. Hold the stick still while it runs.
//
// The numbers are the raw starting point for compensation later: the firmware
// knows each coil's current, so a known disturbance per coil can be subtracted.
// Runs as a state machine from the main loop (~3.5 s), USB stays alive.
// =============================================================================

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int16_t dx, dy, dz;   // TMAG change, raw counts (±300 mT range)
    int16_t dang;         // AS5047 change, 14-bit counts (16384 = 360°)
    uint8_t ok;           // bit 0 TMAG had samples, bit 1 AS5047 had samples
} interf_result_t;

typedef struct {
    int16_t  x, y, z;     // baseline TMAG
    uint16_t ang;         // baseline AS5047 angle
    uint8_t  ok;          // same bits as above, for the baseline
} interf_base_t;

void interf_start(void);
bool interf_running(void);
bool interf_task(void);                       // true once, when finished
const interf_result_t *interf_results(void);  // DRV_COUNT entries
const interf_base_t   *interf_baseline(void);

#endif // COIL_INTERF_H
