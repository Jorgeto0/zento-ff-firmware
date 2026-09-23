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

uint16_t current_raw(drv_id_t id) {
    if (id >= DRV_COUNT) return 0;
    adc_select_input(adc_ch[id]);
    return adc_read() & 0x0FFF;
}

float current_amps(drv_id_t id) {
    // Provisional: 3.3V over 4095 counts, ~1.65 V per amp through the
    // mirror and sense resistor. Verify against a known load before
    // trusting the absolute value.
    return ((float)current_raw(id) / 4095.0f) * 2.0f;
}
