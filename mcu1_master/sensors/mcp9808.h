#ifndef MCP9808_H
#define MCP9808_H

// =============================================================================
// mcu1_master/sensors/mcp9808.h — MCP9808 temperature sensor
// U25 on the main board, I2C1 (GPIO10 SDA, GPIO11 SCL), pull-ups fitted.
//
// Useful as a diagnostic as well as a sensor: I2C is a completely separate
// bus from the SPI peripherals, so a successful read shows the MCU and the
// main board are healthy even while the SPI side is failing.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>

#define MCP9808_ADDR        0x18    // A0-A2 tied low
#define MCP9808_REG_TEMP    0x05
#define MCP9808_REG_MFR_ID  0x06    // always reads 0x0054
#define MCP9808_REG_DEV_ID  0x07    // always reads 0x0400

typedef enum {
    MCP_OK          = 0,
    MCP_ERR_NACK    = 1,    // no device answered at that address
    MCP_ERR_ID      = 2,    // answered but the ID was wrong
} mcp_result_t;

mcp_result_t mcp9808_init(void);
mcp_result_t mcp9808_read_temp(float *celsius);
uint16_t     mcp9808_last_id(void);   // raw manufacturer ID, for diagnosis

#endif // MCP9808_H
