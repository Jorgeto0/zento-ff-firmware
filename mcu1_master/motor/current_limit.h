#ifndef CURRENT_LIMIT_H
#define CURRENT_LIMIT_H

// =============================================================================
// mcu1_master/motor/current_limit.h — per-coil current limit
//
// Each coil has a limit in milliamps (0 = no limit). Every 2 ms the measured
// coil current is compared with the limit and the PWM output is scaled down
// until it sits at the limit. The host's requested force is never exceeded;
// the limit only reduces it. The stick duty cap in coil_pwm.h still applies on
// top as a hard ceiling for the stick coils.
//
// Defaults at boot: 500 mA on coils 1-4, no limit on the voice coil.
// Set from the host with config command 0x11; saved to flash by
// coil_settings.c and loaded again at boot.
// =============================================================================

#include <stdint.h>
#include "drv8873.h"

#define CURRENT_LIMIT_PERIOD_US   2000u

void     current_limit_set_ma(drv_id_t id, uint16_t ma);
uint16_t current_limit_get_ma(drv_id_t id);

// Call from the main loop; runs itself every CURRENT_LIMIT_PERIOD_US.
void     current_limit_task(void);

#endif // CURRENT_LIMIT_H
