#include "current_sense.h"
#include "config.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

// ADC input index per driver, in drv_id_t order: COIL1..COIL4 then VC1
static const uint8_t adc_ch[DRV_COUNT] = { 4, 5, 6, 7, 3 };
static const uint8_t adc_gpio[DRV_COUNT] = { 44, 45, 46, 47, 43 };

void current_sense_init(void) {
    adc_init();
    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        adc_gpio_init(adc_gpio[i]);
    }
}

// IPROPI mirrors the high-side FET current (DRV8873 datasheet). Average 64
// samples (~130 us, 20 samples per 25 kHz PWM period) to smooth ripple and
// ADC noise. Datasheet accuracy: +/-50 mA below 1 A, +/-5% above.
#define CURRENT_AVG_SAMPLES 64u

uint16_t current_raw(drv_id_t id) {
    if (id >= DRV_COUNT) return 0;
    adc_select_input(adc_ch[id]);
    uint32_t sum = 0;
    for (uint32_t n = 0; n < CURRENT_AVG_SAMPLES; n++) {
        sum += adc_read() & 0x0FFF;
    }
    return (uint16_t)(sum / CURRENT_AVG_SAMPLES);
}

float current_amps(drv_id_t id) {
    // I_load = V_adc / R_sense * 1100
    //   mirror ratio 1/1100 (DRV8873 datasheet, IPROPI)
    //   R_sense 1.5 kOhm (R18, R32, R49, R52, R67)
    //   3.3 V over 4095 counts -> 2.42 A full scale, 0.000591 A per count
    const float volts = (float)current_raw(id) * (3.3f / 4095.0f);
    return volts / 1500.0f * 1100.0f;
}

uint16_t current_coil_ma(drv_id_t id) {
    if (id >= DRV_COUNT) return 0;
    // No duty correction: coil_pwm.c brakes on the high side (IN1=IN2=1,
    // OUT1=OUT2=H) between drive pulses, so IPROPI keeps mirroring the coil
    // current the whole period. The averaged reading is the coil current.
    float ma = current_amps(id) * 1000.0f;
    if (ma > 65535.0f) ma = 65535.0f;
    return (uint16_t)(ma + 0.5f);
}
