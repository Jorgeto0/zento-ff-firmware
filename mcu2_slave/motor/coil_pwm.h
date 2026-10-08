#ifndef COIL_PWM_H
#define COIL_PWM_H

// =============================================================================
// mcu2_slave/motor/coil_pwm.h — PWM force output for the five RIGHT coils
//
// DRV8873 in its default PWM (IN/IN) mode, Table 5: both control pins are
// hardware PWM; see coil_pwm.c for the forward/reverse/coast pattern.
//
// Slice map checked against pico-sdk PWM_GPIO_SLICE_NUM for RP2350:
//   Right board pins (netlist): IN1/IN2 = 25/26, 27/28, 29/30, 31/32, 33/34.
//   Here a coil's two pins sit on DIFFERENT slices (25 = 4B, 26 = 5A ...),
//   so init configures the slice of every pin. All channels are distinct.
//
// 25 kHz: inside the 20-32 kHz target and above audible range.
// =============================================================================

#include <stdint.h>
#include "coil_ids.h"

#define COIL_PWM_HZ     25000u
#define COIL_PWM_WRAP   4999u       // 125 MHz / 5000 = 25 kHz

// Right stick coils: no duty cap here. Their rail (6V_2) is not measured
// yet and its buck carries a "CHANGE FB TO 12v" note, so main.c guards them
// on measured current instead (see STICK_GUARD_MA).
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
