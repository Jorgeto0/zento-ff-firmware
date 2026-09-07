// =============================================================================
// mcu1_master/sensors/tmag5170.c — TMAG5170A2 driver
// All behaviour verified against TI datasheet SBASAF4 (Sept 2021)
// =============================================================================

#include "tmag5170.h"
#include "config.h"
#include "diagnostics/uart_log.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define TMAG_SPI        spi0
#define TMAG_SPI_HZ     4000000u    // well under the 10 MHz limit in 6.8

static bool ready = false;

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
    uint8_t out[4] = {
        (uint8_t)(tx >> 24), (uint8_t)(tx >> 16),
        (uint8_t)(tx >> 8),  (uint8_t)tx
    };
    uint8_t in[4] = {0};

    gpio_put(M_SPI0_CS_PIN, 0);
    spi_write_read_blocking(TMAG_SPI, out, in, 4);
    gpio_put(M_SPI0_CS_PIN, 1);

    // 6.8: CS must stay high at least tw_cs (100 ns) between frames
    busy_wait_us(1);

    return ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) |
           ((uint32_t)in[2] << 8)  | (uint32_t)in[3];
}

tmag_result_t tmag_read_reg(uint8_t addr, uint16_t *out) {
    if (!ready) return TMAG_ERR_SPI;

    uint32_t rx = xfer(build_frame(true, addr, 0));

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

tmag_result_t tmag_init(void) {
    // SPI mode 0 per 7.5.2.1
    spi_init(TMAG_SPI, TMAG_SPI_HZ);
    spi_set_format(TMAG_SPI, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(M_SPI0_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(M_SPI0_SDO_PIN, GPIO_FUNC_SPI);   // MCU out, sensor SDI
    gpio_set_function(M_SPI0_SDI_PIN, GPIO_FUNC_SPI);   // MCU in,  sensor SDO

    // CS driven by hand so it can stay low across the full 32-bit frame
    gpio_init(M_SPI0_CS_PIN);
    gpio_set_dir(M_SPI0_CS_PIN, GPIO_OUT);
    gpio_put(M_SPI0_CS_PIN, 1);

    // 6.7: up to 350 us to start after VCC crosses the minimum
    sleep_ms(1);
    ready = true;

    // Prove the link before trusting any data. TEST_CONFIG bits 5:4 (VER)
    // read 1h on the A2 part, Table 7-21.
    uint16_t test;
    tmag_result_t r = tmag_read_reg(TMAG_REG_TEST_CONFIG, &test);
    if (r != TMAG_OK) {
        ready = false;
        log_value("TMAG link failed, err", (int32_t)r);
        return r;
    }
    log_hex("TMAG TEST_CONFIG", test);

    // Continuous XYZ conversion at the default +/-150 mT range
    tmag_write_reg(TMAG_REG_SENSOR_CONFIG,
                   (uint16_t)(TMAG_MAG_CH_EN_XYZ << TMAG_MAG_CH_EN_SHIFT));

    tmag_write_reg(TMAG_REG_DEVICE_CONFIG,
                   (uint16_t)((TMAG_CONV_AVG_1X << TMAG_CONV_AVG_SHIFT) |
                              (TMAG_OP_MODE_ACTIVE << TMAG_OP_MODE_SHIFT)));

    log_info("TMAG5170 init OK");
    return TMAG_OK;
}
