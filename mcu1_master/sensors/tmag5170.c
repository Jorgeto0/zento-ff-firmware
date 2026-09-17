// =============================================================================
// mcu1_master/sensors/tmag5170.c — TMAG5170A2 driver
// All behaviour verified against TI datasheet SBASAF4 (Sept 2021)
// =============================================================================

#include "tmag5170.h"
#include "config.h"
#include "diagnostics/uart_log.h"
#include "spi_pio.h"
#include "spi_pio_m1.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define TMAG_SPI_HZ     4000000u    // well under the 10 MHz limit in 6.8

// Hardware SPI cannot be used here. On this board GPIO16 carries M_SPI0_SDI,
// which drives the sensor's SDI input, but the RP2350 mux only offers
// SPI0_RX on GPIO16 (checked in pico-sdk io_bank0.h). GPIO19 has the mirror
// problem. PIO drives either pin in either direction, so the board works as
// wired with no rework.
static spi_pio_t bus;
static spi_m1_t  bus_m1;   // proven wrapper, used for the link probe
static bool ready = false;
uint32_t tmag_last_rx = 0;   // raw reply, exposed for diagnostics
uint8_t  tmag_wiring  = 0;   // 0 = not working, 1 = normal pinout, 2 = swapped

// -----------------------------------------------------------------------------
// Frame layout, 7.5.2:
//   [31] R/W  [30:24] address  [23:8] data  [7:4] command  [3:0] CRC
// -----------------------------------------------------------------------------
static uint32_t build_frame(bool read, uint8_t addr, uint16_t data) {
    uint32_t f = 0;
    if (read) f |= (1u << 31);
    f |= ((uint32_t)(addr & 0x7F)) << 24;
    f |= ((uint32_t)data) << 8;
    f |= tmag_crc4(f);              // command bits left at 0
    return f;
}

// Full-duplex 32-bit exchange. CS must stay low for the whole frame, 7.5.2.2.
static uint32_t xfer(uint32_t tx) {
    // CS stays low for the whole 32-bit frame, required by 7.5.2.2
    gpio_put(M_SPI0_CS_PIN, 0);
    uint32_t rx = spi_pio_xfer(&bus, tx);
    gpio_put(M_SPI0_CS_PIN, 1);

    // 6.8: CS must stay high at least tw_cs (100 ns) between frames
    busy_wait_us(1);
    return rx;
}

tmag_result_t tmag_read_reg(uint8_t addr, uint16_t *out) {
    if (!ready) return TMAG_ERR_SPI;

    uint32_t rx = xfer(build_frame(true, addr, 0));
    tmag_last_rx = rx;   // keep the raw frame so a failure can be diagnosed

    // All ones or all zeros means nothing is driving SDO
    if (rx == 0xFFFFFFFFu) return TMAG_ERR_NO_REPLY;

    // Device embeds a CRC in its reply, 7.5.2.5
    if (tmag_crc4(rx) != (rx & 0x0F)) return TMAG_ERR_CRC;

    *out = (uint16_t)((rx >> 8) & 0xFFFF);   // 7.5.2.4.1, regular 32-bit read
    return TMAG_OK;
}

tmag_result_t tmag_write_reg(uint8_t addr, uint16_t value) {
    if (!ready) return TMAG_ERR_SPI;
    xfer(build_frame(false, addr, value));
    return TMAG_OK;
}

tmag_result_t tmag_read_xyz(tmag_xyz_t *out) {
    uint16_t x, y, z;
    tmag_result_t r;

    if ((r = tmag_read_reg(TMAG_REG_X_CH_RESULT, &x)) != TMAG_OK) return r;
    if ((r = tmag_read_reg(TMAG_REG_Y_CH_RESULT, &y)) != TMAG_OK) return r;
    if ((r = tmag_read_reg(TMAG_REG_Z_CH_RESULT, &z)) != TMAG_OK) return r;

    // 7.5.1.1: results are 16-bit 2's complement
    out->x = (int16_t)x;
    out->y = (int16_t)y;
    out->z = (int16_t)z;
    return TMAG_OK;
}

float tmag_to_mt(int16_t raw, uint16_t range_mt) {
    // Equation 1, 7.5.1.1: B = (raw / 2^16) * 2 * range
    return ((float)raw / 32768.0f) * (float)range_mt;
}

// Bring up one wiring option and see if the sensor answers.
// sm picks a distinct state machine so a second attempt does not clash with
// the first program already loaded into the block.
static tmag_result_t try_wiring(uint mosi, uint miso, uint sm) {
    if (!spi_pio_init(&bus, pio1, sm, mosi, miso, M_SPI0_SCK_PIN,
                      TMAG_SPI_HZ, 32)) {
        return TMAG_ERR_SPI;
    }
    ready = true;
    sleep_ms(1);

    uint16_t test;
    return tmag_read_reg(TMAG_REG_TEST_CONFIG, &test);
}

tmag_result_t tmag_init(void) {
    gpio_init(M_SPI0_CS_PIN);
    gpio_set_dir(M_SPI0_CS_PIN, GPIO_OUT);
    gpio_put(M_SPI0_CS_PIN, 1);

    sleep_ms(2);   // 6.7: up to 350 us to start after VCC is good

    // Attempt 1: the wiring the netlist implies. M_SPI0_SDI reaches the
    // sensor's SDI input, so the MCU drives it.
    // Link probe on the mode-1 wrapper first. Wrong SPI mode for this sensor,
    // so the data will be garbage - but garbage proves the sensor is alive and
    // the pins are right. The mode-1 wrapper is the only one proven on this
    // hardware (VC1 uses it); the mode-0 one has never had a reply.
    if (spi_m1_init(&bus_m1, pio1, 2, M_SPI0_SDI_PIN, M_SPI0_SDO_PIN,
                    M_SPI0_SCK_PIN, TMAG_SPI_HZ)) {
        gpio_put(M_SPI0_CS_PIN, 0);
        uint32_t probe_hi = spi_m1_xfer(&bus_m1, 0x8F00, 16);
        uint32_t probe_lo = spi_m1_xfer(&bus_m1, 0x0008, 16);
        gpio_put(M_SPI0_CS_PIN, 1);
        tmag_last_rx = (probe_hi << 16) | (probe_lo & 0xFFFF);
        log_hex("TMAG mode1 probe", tmag_last_rx);
    }

    tmag_result_t r = try_wiring(M_SPI0_SDI_PIN, M_SPI0_SDO_PIN, 0);
    if (r == TMAG_OK) {
        tmag_wiring = 1;
    } else {
        // Attempt 2: swapped. CN8 pin numbering differs between the two
        // schematic sheets, so a straight-through cable would cross these.
        // A cross leaves our MISO input facing the sensor's SDI input, which
        // floats and reads all zeros - exactly the symptom.
        ready = false;
        r = try_wiring(M_SPI0_SDO_PIN, M_SPI0_SDI_PIN, 3);
        if (r == TMAG_OK) {
            tmag_wiring = 2;
        }
    }

    if (r != TMAG_OK) {
        ready = false;
        tmag_wiring = 0;
        log_value("TMAG both wirings failed, err", (int32_t)r);
        return r;
    }

    log_value("TMAG alive, wiring", tmag_wiring);

    tmag_write_reg(TMAG_REG_SENSOR_CONFIG,
                   (uint16_t)(TMAG_MAG_CH_EN_XYZ << TMAG_MAG_CH_EN_SHIFT));
    tmag_write_reg(TMAG_REG_DEVICE_CONFIG,
                   (uint16_t)((TMAG_CONV_AVG_1X << TMAG_CONV_AVG_SHIFT) |
                              (TMAG_OP_MODE_ACTIVE << TMAG_OP_MODE_SHIFT)));
    log_info("TMAG5170 init OK");
    return TMAG_OK;
}
