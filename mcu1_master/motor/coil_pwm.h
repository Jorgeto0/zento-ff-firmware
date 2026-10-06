#ifndef COIL_PWM_H
#define COIL_PWM_H

// =============================================================================
// mcu1_master/motor/coil_pwm.h — PWM force output for the five left coils
//
// DRV8873 in its default PWM (IN/IN) mode, Table 5: both control pins are
// hardware PWM; see coil_pwm.c for the forward/reverse/coast pattern.
//
// Slice map checked against pico-sdk PWM_GPIO_SLICE_NUM for RP2350:
//   GPIO28/29 slice 6A/B, 30/31 7A/B, 32/33 8A/B, 34/35 9A/B, 36/37 10A/B.
//   All distinct, so the five coils are independent.
//
// 25 kHz: inside the 20-32 kHz target and above audible range.
// =============================================================================

#include <stdint.h>
#include "drv8873.h"

#define COIL_PWM_HZ     25000u
#define COIL_PWM_WRAP   4999u       // 125 MHz / 5000 = 25 kHz

// Stick coil duty cap. Was 50% on the assumption the stick rail (6V_1) was
// a bypassed 12V. Measured on the board: 4.05V across a coil at 50%, so the
// rail is ~8V. At 100% that is ~8V / 15.5 ohm = ~0.52A per coil, which the
// client confirmed is fine for these coils. So no cap for now.
// Lower this again if the rail changes (e.g. buck back to 6V or up to 12V).
#define COIL_MAX_DUTY   COIL_PWM_WRAP     // 100%

void coil_pwm_init(void);

// force: -32768 to +32767. Sign sets direction, magnitude sets duty.
// Zero: both inputs low, outputs Hi-Z (coast), no current (Table 5).
void coil_set_force(drv_id_t id, int16_t force);

// Stop every coil. Called on fault or loss of host contact.
void coil_all_off(void);

// Read back the PWM level last written, for diagnosis.
uint16_t coil_get_level(drv_id_t id);

// Level the host asked for, before the current limit scales it down.
uint16_t coil_get_cmd_level(drv_id_t id);

// Current-limit scale, Q16 (65536 = 1.0). Output = requested level x scale.
#define COIL_SCALE_ONE  65536u
void     coil_set_scale_q16(drv_id_t id, uint32_t q16);
uint32_t coil_get_scale_q16(drv_id_t id);

#endif // COIL_PWM_H
