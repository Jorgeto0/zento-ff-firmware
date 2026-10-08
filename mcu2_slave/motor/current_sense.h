#ifndef CURRENT_SENSE_H
#define CURRENT_SENSE_H

// =============================================================================
// mcu2_slave/motor/current_sense.h — coil current via the MCU's own ADC
//
// Each DRV8873 mirrors its bridge current out of IPROPI1/IPROPI2 into a sense
// resistor. Both pins tie to the same net, so the reading is the bridge total.
//
// Mapping, from the netlist:
//   VC1    GPIO43 ADC3      COIL3  GPIO46 ADC6
//   COIL1  GPIO44 ADC4      COIL4  GPIO47 ADC7
//   COIL2  GPIO45 ADC5
//
// Scaling (verified): 1/1100 mirror (DRV8873 datasheet) into 1.5 kOhm
// (R18/R32/R49/R52/R67), 3.3 V over 4095 counts -> 0.000591 A per count.
//
// IPROPI mirrors the high-side FETs. In PH/EN the PWM off-time is high-side
// recirculation (Table 4), so the coil current stays visible on IPROPI the
// whole period; no duty correction is needed. Accuracy is +/-50 mA below 1 A
// and the datasheet's monitoring range starts at 100 mA.
// =============================================================================

#include <stdint.h>
#include "coil_ids.h"

void     current_sense_init(void);
uint16_t current_raw(drv_id_t id);      // 0-4095, straight from the ADC
float    current_amps(drv_id_t id);     // averaged high-side (supply) current
uint16_t current_coil_ma(drv_id_t id);  // coil current, milliamps

#endif // CURRENT_SENSE_H
