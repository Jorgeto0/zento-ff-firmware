#include "lsm6dsl.h"
#include "spi_pio_m3.h"
#include "config.h"
#include "diagnostics/uart_log.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define LSM_SPI_HZ  1000000u    // 1 MHz; ringing on this bus is a known issue

static spi_m3_t bus;
static bool     ready   = false;
static uint8_t  whoami  = 0;

uint8_t lsm6dsl_last_whoami(void) { return whoami; }

static uint8_t read_reg(uint8_t reg) {
    gpio_put(M_SPI1_CS_PIN, 0);
    spi_m3_xfer(&bus, (uint32_t)(0x80u | (reg & 0x7F)), 8);
    uint32_t v = spi_m3_xfer(&bus, 0x00, 8);
    gpio_put(M_SPI1_CS_PIN, 1);
    busy_wait_us(1);
    return (uint8_t)(v & 0xFF);
}

static void write_reg(uint8_t reg, uint8_t val) {
    gpio_put(M_SPI1_CS_PIN, 0);
    spi_m3_xfer(&bus, (uint32_t)(reg & 0x7F), 8);
    spi_m3_xfer(&bus, val, 8);
    gpio_put(M_SPI1_CS_PIN, 1);
    busy_wait_us(1);
}

lsm_result_t lsm6dsl_read_gyro(lsm_xyz_t *out) {
    if (!ready) return LSM_ERR_SPI;
    uint8_t b[6];
    for (uint8_t i = 0; i < 6; i++) {
        b[i] = read_reg((uint8_t)(LSM_REG_OUTX_L_G + i));
    }
    out->x = (int16_t)((b[1] << 8) | b[0]);
    out->y = (int16_t)((b[3] << 8) | b[2]);
    out->z = (int16_t)((b[5] << 8) | b[4]);
    return LSM_OK;
}

lsm_result_t lsm6dsl_init(void) {
    // SPI1 uses pio1 SM 1 here; the AS5047 no longer holds it in this build
    if (!spi_m3_init(&bus, pio1, 1, M_SPI1_SDI_PIN, M_SPI1_SDO_PIN,
                     M_SPI1_SCK_PIN, LSM_SPI_HZ)) {
        log_error("LSM6DSL PIO SPI init failed");
        return LSM_ERR_SPI;
    }
    gpio_init(M_SPI1_CS_PIN);
    gpio_set_dir(M_SPI1_CS_PIN, GPIO_OUT);
    gpio_put(M_SPI1_CS_PIN, 1);

    sleep_ms(20);    // boot time after power on
    ready = true;

    whoami = read_reg(LSM_REG_WHO_AM_I);
    log_hex("LSM6DSL WHO_AM_I", whoami);
    if (whoami != LSM_WHO_AM_I_VAL) {
        ready = false;
        return LSM_ERR_WHOAMI;
    }

    // Gyro at 104 Hz, 2000 dps full scale
    write_reg(LSM_REG_CTRL2_G, 0x4C);
    log_info("LSM6DSL init OK");
    return LSM_OK;
}
