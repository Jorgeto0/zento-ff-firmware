#include "mcp9808.h"
#include "config.h"
#include "diagnostics/uart_log.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#define MCP_I2C     i2c1
#define MCP_HZ      100000u

static bool     ready   = false;
static uint16_t last_id = 0;

uint16_t mcp9808_last_id(void) { return last_id; }

static mcp_result_t read_reg(uint8_t reg, uint16_t *out) {
    if (!ready) return MCP_ERR_NACK;

    if (i2c_write_blocking(MCP_I2C, MCP9808_ADDR, &reg, 1, true) < 0) {
        return MCP_ERR_NACK;
    }
    uint8_t rx[2];
    if (i2c_read_blocking(MCP_I2C, MCP9808_ADDR, rx, 2, false) < 0) {
        return MCP_ERR_NACK;
    }
    *out = (uint16_t)((rx[0] << 8) | rx[1]);
    return MCP_OK;
}

mcp_result_t mcp9808_read_temp(float *celsius) {
    uint16_t raw;
    mcp_result_t r = read_reg(MCP9808_REG_TEMP, &raw);
    if (r != MCP_OK) return r;

    // Datasheet 5.1.3: bits 12:0 are the temperature, bit 12 is the sign,
    // resolution 0.0625 C per LSB
    int16_t t = (int16_t)(raw & 0x0FFF);
    if (raw & 0x1000) t -= 4096;          // negative
    *celsius = (float)t * 0.0625f;
    return MCP_OK;
}

mcp_result_t mcp9808_init(void) {
    i2c_init(MCP_I2C, MCP_HZ);
    gpio_set_function(M_I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(M_I2C1_SCL_PIN, GPIO_FUNC_I2C);
    // External pull-ups are fitted (R27, R24) but enabling the internal ones
    // costs nothing and helps if a board is missing them.
    gpio_pull_up(M_I2C1_SDA_PIN);
    gpio_pull_up(M_I2C1_SCL_PIN);

    ready = true;

    mcp_result_t r = read_reg(MCP9808_REG_MFR_ID, &last_id);
    if (r != MCP_OK) {
        ready = false;
        log_info("MCP9808 no reply on I2C1");
        return r;
    }
    log_hex("MCP9808 mfr id", last_id);

    if (last_id != 0x0054) {
        return MCP_ERR_ID;
    }
    log_info("MCP9808 init OK");
    return MCP_OK;
}
