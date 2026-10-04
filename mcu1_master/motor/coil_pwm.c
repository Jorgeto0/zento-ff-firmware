#include "coil_pwm.h"
#include "config.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"

static const uint8_t pwm_pin[DRV_COUNT] = {
    M_PWM_COIL1_PIN, M_PWM_COIL2_PIN, M_PWM_COIL3_PIN,
    M_PWM_COIL4_PIN, M_PWM_VC1_PIN
};
static const uint8_t dir_pin[DRV_COUNT] = {
    M_DIR_COIL1_PIN, M_DIR_COIL2_PIN, M_DIR_COIL3_PIN,
    M_DIR_COIL4_PIN, M_DIR_VC1_PIN
};

// cmd_level: what the host asked for (after the stick duty cap).
// scale_q16: current-limit scale, 65536 = 1.0, set by current_limit.c.
// last_level: what is actually on the pin = cmd_level * scale.
static uint16_t cmd_level[DRV_COUNT]  = {0};
static uint32_t scale_q16[DRV_COUNT];
static uint16_t last_level[DRV_COUNT] = {0};

static void apply(drv_id_t id) {
    uint16_t out = (uint16_t)(((uint32_t)cmd_level[id] * scale_q16[id]) >> 16);
    last_level[id] = out;
    pwm_set_gpio_level(pwm_pin[id], out);
}

void coil_pwm_init(void) {
    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        scale_q16[i] = COIL_SCALE_ONE;

        gpio_set_function(pwm_pin[i], GPIO_FUNC_PWM);
        uint slice = pwm_gpio_to_slice_num(pwm_pin[i]);
        pwm_set_wrap(slice, COIL_PWM_WRAP);
        pwm_set_gpio_level(pwm_pin[i], 0);      // start with no force
        pwm_set_enabled(slice, true);

        gpio_init(dir_pin[i]);
        gpio_set_dir(dir_pin[i], GPIO_OUT);
        gpio_put(dir_pin[i], 0);
    }
}

void coil_set_force(drv_id_t id, int16_t force) {
    if (id >= DRV_COUNT) return;

    // Direction from the sign, magnitude from the value. -32768 has no
    // positive counterpart, so clamp it before negating.
    bool reverse = (force < 0);
    uint32_t mag = reverse ? (uint32_t)(-(int32_t)force) : (uint32_t)force;
    if (mag > 32767u) mag = 32767u;

    gpio_put(dir_pin[id], reverse ? 1 : 0);
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
        pwm_set_gpio_level(pwm_pin[i], 0);
    }
}

uint16_t coil_get_level(drv_id_t id) {
    return (id < DRV_COUNT) ? last_level[id] : 0;
}

uint16_t coil_get_cmd_level(drv_id_t id) {
    return (id < DRV_COUNT) ? cmd_level[id] : 0;
}
