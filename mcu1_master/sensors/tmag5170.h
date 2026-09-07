#ifndef TMAG5170_H
#define TMAG5170_H

// =============================================================================
// mcu1_master/sensors/tmag5170.h — TMAG5170A2 3D Hall sensor
// Part on this board: TMAG5170A2EDGKRQ1, on the hall sensor daughter board
// Bus: SPI0, via connector CN8
// All facts below verified against TI datasheet SBASAF4 (Sept 2021)
// =============================================================================
// SPI mode 0 (CPOL=0, CPHA=0) per 7.5.2.1: SDO transitions on the falling edge
// of SCK, SDI is latched on the rising edge. Max 10 MHz per 6.8.
//
// Fixed 32-bit frame per 7.5.2, laid out as:
//   [31]     R/W       1 = read, 0 = write
//   [30:24]  A6-A0     7-bit register address
//   [23:8]   D15-D0    16-bit data (don't care on a read)
//   [7:4]    CMD3-0    command bits
//   [3:0]    CRC       4-bit CRC, polynomial x^4+x+1, init 0b1111
//
// CRC is MANDATORY at power up. 7.5.2.5: "the device will ignore all the SDI
// commands if proper CRC codes are not received."
// =============================================================================

#include <stdint.h>
#include <stdbool.h>

#define TMAG_SPI_MAX_HZ         10000000u   // 6.8, fSPI max
#define TMAG_CRC_DISABLE_FRAME  0x0F000407u // 7.5.2.5, disables SDI CRC

// -----------------------------------------------------------------------------
// Register map — Table 7-4
// -----------------------------------------------------------------------------
#define TMAG_REG_DEVICE_CONFIG      0x00
#define TMAG_REG_SENSOR_CONFIG      0x01
#define TMAG_REG_SYSTEM_CONFIG      0x02
#define TMAG_REG_ALERT_CONFIG       0x03
#define TMAG_REG_CONV_STATUS        0x08
#define TMAG_REG_X_CH_RESULT        0x09
#define TMAG_REG_Y_CH_RESULT        0x0A
#define TMAG_REG_Z_CH_RESULT        0x0B
#define TMAG_REG_TEMP_RESULT        0x0C
#define TMAG_REG_AFE_STATUS         0x0D
#define TMAG_REG_SYS_STATUS         0x0E
#define TMAG_REG_TEST_CONFIG        0x0F
#define TMAG_REG_ANGLE_RESULT       0x13
#define TMAG_REG_MAGNITUDE_RESULT   0x14

// -----------------------------------------------------------------------------
// DEVICE_CONFIG (0x00) — Table 7-6
// -----------------------------------------------------------------------------
#define TMAG_CONV_AVG_SHIFT         12      // bits 14:12
#define TMAG_CONV_AVG_1X            0x0
#define TMAG_CONV_AVG_32X           0x5     // best SNR

#define TMAG_OP_MODE_SHIFT          4       // bits 6:4
#define TMAG_OP_MODE_CONFIG         0x0     // default at power up
#define TMAG_OP_MODE_STANDBY        0x1
#define TMAG_OP_MODE_ACTIVE         0x2     // continuous conversion
#define TMAG_OP_MODE_ACTIVE_TRIG    0x3

// -----------------------------------------------------------------------------
// SENSOR_CONFIG (0x01) — Table 7-7
// -----------------------------------------------------------------------------
#define TMAG_MAG_CH_EN_SHIFT        6       // bits 9:6
#define TMAG_MAG_CH_EN_NONE         0x0
#define TMAG_MAG_CH_EN_XYZ          0x7

// Range bits: Z 5:4, Y 3:2, X 1:0. A2 part per Table 7-1.
#define TMAG_RANGE_150MT            0x0     // default
#define TMAG_RANGE_75MT             0x1     // best resolution
#define TMAG_RANGE_300MT            0x2     // widest

// -----------------------------------------------------------------------------
// Return codes
// -----------------------------------------------------------------------------
typedef enum {
    TMAG_OK             = 0,
    TMAG_ERR_SPI        = 1,   // bus not initialised
    TMAG_ERR_CRC        = 2,   // CRC mismatch on the reply
    TMAG_ERR_NO_REPLY   = 3,   // SDO stuck high or low
} tmag_result_t;

// -----------------------------------------------------------------------------
// Raw three-axis reading, straight from the result registers.
// 16-bit 2's complement per 7.5.1.1. Convert to mT with tmag_to_mt().
// -----------------------------------------------------------------------------
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
} tmag_xyz_t;

// -----------------------------------------------------------------------------
// tmag_crc4()
// 4-bit CRC over the top 28 bits of a frame, per 7.5.2.5.
// Polynomial x^4+x+1, initialised to 0b1111, low 4 bits zeroed before feeding.
// -----------------------------------------------------------------------------
uint8_t tmag_crc4(uint32_t frame);

// Bring up SPI0 and configure the sensor for continuous XYZ conversion.
tmag_result_t tmag_init(void);

// Read one register. Returns the 16-bit data field.
tmag_result_t tmag_read_reg(uint8_t addr, uint16_t *out);

// Write one register.
tmag_result_t tmag_write_reg(uint8_t addr, uint16_t value);

// Read all three axes.
tmag_result_t tmag_read_xyz(tmag_xyz_t *out);

// Convert a raw axis reading to millitesla. range_mt is 75, 150 or 300.
float tmag_to_mt(int16_t raw, uint16_t range_mt);

#endif // TMAG5170_H
