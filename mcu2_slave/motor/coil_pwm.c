#include "coil_pwm.h"
#include "config.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

static const uint8_t pwm_pin[DRV_COUNT] = {
    S_PWM_COIL1_PIN, S_PWM_COIL2_PIN, S_PWM_COIL3_PIN,
    S_PWM_COIL4_PIN, S_PWM_VC1_PIN
};
static const uint8_t dir_pin[DRV_COUNT] = {
    S_DIR_COIL1_PIN, S_DIR_COIL2_PIN, S_DIR_COIL3_PIN,
    S_DIR_COIL4_PIN, S_DIR_VC1_PIN
};

// Per-coil direction. On the RIGHT board coil wire 1 goes to OUT1 and wire 2
// to OUT2 (netlist), the opposite of the left. Flip entries here once the
// client confirms which right coils push and which pull.
// true = flip the sign so every coil does the same thing for the same force.
static const bool invert[DRV_COUNT] = {
    false,   // COIL1
    false,   // COIL2
    false,   // COIL3 — right side polarity not confirmed yet
    false,   // COIL4
    false    // VC1
};

// Drive scheme: the drivers run in their power-up PWM (IN/IN) mode, Table 5:
//   IN1 IN2 -> OUT1 OUT2
//    0   0  -> Hi-Z Hi-Z   coast (used for zero force)
//    0   1  ->  L    H     drive, reverse
//    1   0  ->  H    L     drive, forward
//    1   1  ->  H    H     high-side brake (slow decay)
// EN/IN1 is on the slice's A channel and PH/IN2 on its B channel (GPIO28/29,
// 30/31, 32/33, 34/35, 36/37), so both are hardware PWM.
// Forward: IN1 held high, IN2 pulses; drive while IN2 is low, high-side brake
// while it is high. Reverse is the mirror. The coil current never coasts away
// between pulses, so force is proportional to duty in both directions, and
// IPROPI (high-side current) stays valid the whole period.
// Measured on the board 2026-10-06: an idle coil with its PH pin high drew
// 0.25A and reverse gave 0A, which only PWM mode explains. Our SPI mode write
// is not taking effect, so the firmware no longer relies on it.

// cmd_level: what the host asked for (after the stick duty cap).
// scale_q16: current-limit scale, 65536 = 1.0, set by current_limit.c.
// last_level: drive duty actually applied = cmd_level * scale (0..WRAP).
static uint16_t cmd_level[DRV_COUNT]  = {0};
static uint32_t scale_q16[DRV_COUNT];
static uint16_t last_level[DRV_COUNT] = {0};
static bool     rev[DRV_COUNT]        = {false};

#define LEVEL_ALWAYS_HIGH   (COIL_PWM_WRAP + 1u)   // level > wrap = 100% high

static void apply(drv_id_t id) {
    uint16_t out = (uint16_t)(((uint32_t)cmd_level[id] * scale_q16[id]) >> 16);
    last_level[id] = out;

    if (out == 0) {                          // coast: both inputs low
        pwm_set_gpio_level(pwm_pin[id], 0);
        pwm_set_gpio_level(dir_pin[id], 0);
        return;
    }
    // The pulsing input is low for 'out' counts (drive) and high for the
    // rest (brake): level = high time = period - out.
    uint16_t pulse = (uint16_t)(LEVEL_ALWAYS_HIGH - out);
    if (!rev[id]) {
        pwm_set_gpio_level(pwm_pin[id], LEVEL_ALWAYS_HIGH);   // IN1 = 1
        pwm_set_gpio_level(dir_pin[id], pulse);               // IN2 pulses
    } else {
        pwm_set_gpio_level(dir_pin[id], LEVEL_ALWAYS_HIGH);   // IN2 = 1
        pwm_set_gpio_level(pwm_pin[id], pulse);               // IN1 pulses
    }
}

void coil_pwm_init(void) {
    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        scale_q16[i] = COIL_SCALE_ONE;
        rev[i] = false;

        gpio_set_function(pwm_pin[i], GPIO_FUNC_PWM);
        gpio_set_function(dir_pin[i], GPIO_FUNC_PWM);
        // Right board: the two pins can be on different slices, so set up
        // the slice of each one (same wrap everywhere, 25 kHz).
        uint s1 = pwm_gpio_to_slice_num(pwm_pin[i]);
        uint s2 = pwm_gpio_to_slice_num(dir_pin[i]);
        pwm_set_wrap(s1, COIL_PWM_WRAP);
        pwm_set_wrap(s2, COIL_PWM_WRAP);
        pwm_set_gpio_level(pwm_pin[i], 0);   // coast at boot: no force
        pwm_set_gpio_level(dir_pin[i], 0);
        pwm_set_enabled(s1, true);
        pwm_set_enabled(s2, true);
    }
}

void coil_set_force(drv_id_t id, int16_t force) {
    if (id >= DRV_COUNT) return;

    // Magnitude from the value (-32768 clamped), direction from the sign
    // after the per-coil polarity correction.
    bool neg = (force < 0);
    uint32_t mag = neg ? (uint32_t)(-(int32_t)force) : (uint32_t)force;
    if (mag > 32767u) mag = 32767u;
    rev[id] = (neg != invert[id]);

    uint16_t lvl = (uint16_t)((mag * COIL_PWM_WRAP) / 32767u);
    // Stick coils use the cap in coil_pwm.h; the 12V voice coil never does.
    if (id != DRV_VC1 && lvl > COIL_MAX_DUTY) lvl = COIL_MAX_DUTY;
    cmd_level[id] = lvl;
    apply(id);
}

void coil_set_scale_q16(drv_id_t id, uint32_t q16) {
    if (id >= DRV_COUNT) return;
    if (q16 > COIL_SCALE_ONE) q16 = COIL_SCALE_ONE;
    scale_q16[id] = q16;
    apply(id);
}

uint32_t coil_get_scale_q16(drv_id_t id) {
    return (id < DRV_COUNT) ? scale_q16[id] : COIL_SCALE_ONE;
}

void coil_all_off(void) {
    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        cmd_level[i]  = 0;
        last_level[i] = 0;      // keep readback honest after a timeout
        pwm_set_gpio_level(pwm_pin[i], 0);   // coast
        pwm_set_gpio_level(dir_pin[i], 0);
    }
}

uint16_t coil_get_level(drv_id_t id) {
    return (id < DRV_COUNT) ? last_level[id] : 0;
}

uint16_t coil_get_cmd_level(drv_id_t id) {
    return (id < DRV_COUNT) ? cmd_level[id] : 0;
}
