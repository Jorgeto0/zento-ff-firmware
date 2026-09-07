// =============================================================================
// mcu1_master/sensors/as5047p.c — AS5047P driver
// Verified against ams datasheet DS000324 v2-00
// =============================================================================

#include "as5047p.h"
#include "spi_pio_m1.h"
#include "config.h"
#include "diagnostics/uart_log.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define AS_SPI_HZ   4000000u    // datasheet allows up to 10 MHz

static spi_m1_t bus;
static bool ready = false;

// Even parity over the low 15 bits, placed in bit 15
static uint16_t with_parity(uint16_t v) {
    uint16_t x = v & 0x7FFF;
    uint16_t p = x;
    p ^= p >> 8; p ^= p >> 4; p ^= p >> 2; p ^= p >> 1;
    return (uint16_t)(x | ((p & 1u) << 15));
}

// A valid frame has an even number of set bits across all 16
static bool parity_ok(uint16_t v) {
    uint16_t p = v;
    p ^= p >> 8; p ^= p >> 4; p ^= p >> 2; p ^= p >> 1;
    return (p & 1u) == 0;
}

static uint16_t frame(uint16_t raw) {
    // CS low for the whole 16-bit frame, then tCSn high 350 ns minimum
    gpio_put(M_SPI1_CS_AS_PIN, 0);
    uint16_t rx = (uint16_t)spi_m1_xfer(&bus, raw, 16);
    gpio_put(M_SPI1_CS_AS_PIN, 1);
    busy_wait_us(1);
    return rx;
}

as_result_t as5047_read_reg(uint16_t addr, uint16_t *out) {
    if (!ready) return AS_ERR_SPI;

    // First frame carries the address, second clocks the answer back
    frame(with_parity((uint16_t)(0x4000u | (addr & 0x3FFF))));
    uint16_t rx = frame(with_parity((uint16_t)(0x4000u | AS_REG_NOP)));

    if (!parity_ok(rx))      return AS_ERR_PARITY;
    if (rx & 0x4000u)        return AS_ERR_FLAG;   // EF, Figure 14

    *out = (uint16_t)(rx & 0x3FFF);
    return AS_OK;
}

as_result_t as5047_read_angle(uint16_t *out) {
    return as5047_read_reg(AS_REG_ANGLECOM, out);
}

as_result_t as5047_read_diag(as_diag_t *out) {
    uint16_t v;
    as_result_t r = as5047_read_reg(AS_REG_DIAAGC, &v);
    if (r != AS_OK) return r;

    out->agc  = (uint8_t)(v & AS_DIAAGC_AGC_MASK);
    out->lf   = (v & AS_DIAAGC_LF)   != 0;
    out->cof  = (v & AS_DIAAGC_COF)  != 0;
    out->magh = (v & AS_DIAAGC_MAGH) != 0;
    out->magl = (v & AS_DIAAGC_MAGL) != 0;
    return AS_OK;
}

float as5047_to_degrees(uint16_t raw) {
    return ((float)(raw & 0x3FFF) / 16384.0f) * 360.0f;
}

as_result_t as5047_init(void) {
    // Daughter board U1: pin 4 MOSI is an input, pin 3 MISO an output, so
    // M_SPI1_SDI is the MCU's MOSI and M_SPI1_SDO is its MISO.
    // PIO state machine 1 on pio1 — SM 0 is the TMAG.
    if (!spi_m1_init(&bus, pio1, 1,
                     M_SPI1_SDI_PIN,    // MOSI, into the sensor's MOSI pin
                     M_SPI1_SDO_PIN,    // MISO, from the sensor's MISO pin
                     M_SPI1_SCK_PIN,
                     AS_SPI_HZ)) {
        log_error("AS5047 PIO SPI init failed");
        return AS_ERR_SPI;
    }

    gpio_init(M_SPI1_CS_AS_PIN);
    gpio_set_dir(M_SPI1_CS_AS_PIN, GPIO_OUT);
    gpio_put(M_SPI1_CS_AS_PIN, 1);

    sleep_ms(10);   // tpon, 10 ms to first valid angle (Figure 9)
    ready = true;

    // Clear any startup error flags, then prove the link with DIAAGC
    uint16_t junk;
    as5047_read_reg(AS_REG_ERRFL, &junk);

    as_diag_t d;
    as_result_t r = as5047_read_diag(&d);
    if (r != AS_OK) {
        ready = false;
        log_value("AS5047 link failed, err", (int32_t)r);
        return r;
    }

    log_value("AS5047 AGC", d.agc);
    if (d.magl) log_warning("AS5047 magnetic field too weak");
    if (d.magh) log_warning("AS5047 magnetic field too strong");

    log_info("AS5047P init OK");
    return AS_OK;
}
