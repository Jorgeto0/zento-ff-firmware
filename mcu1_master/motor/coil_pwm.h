#ifndef COIL_PWM_H
#define COIL_PWM_H

// =============================================================================
// mcu1_master/motor/coil_pwm.h — PWM force output for the five left coils
//
// DRV8873 in PH/EN mode (Table 4): EN/IN1 takes the PWM, PH/IN2 sets
// direction. So magnitude drives duty cycle and sign drives the DIR pin.
//
// Slice map checked against pico-sdk PWM_GPIO_SLICE_NUM for RP2350:
//   GPIO28 slice 6A, 30 slice 7A, 32 slice 8A, 34 slice 9A, 36 slice 10A.
//   All distinct, so the five coils are independent.
//
// 25 kHz: inside the 20-32 kHz target and above audible range.
// =============================================================================

#include <stdint.h>
#include "drv8873.h"

#define COIL_PWM_HZ     25000u
#define COIL_PWM_WRAP   4999u       // 125 MHz / 5000 = 25 kHz

void coil_pwm_init(void);

// force: -32768 to +32767. Sign sets direction, magnitude sets duty.
// Zero coasts (EN low), which is Hi-Z on both outputs per Table 4.
void coil_set_force(drv_id_t id, int16_t force);

// Stop every coil. Called on fault or loss of host contact.
void coil_all_off(void);

// Read back the PWM level last written, for diagnosis.
uint16_t coil_get_level(drv_id_t id);

#endif // COIL_PWM_H
