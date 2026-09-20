#ifndef LSM6DSL_H
#define LSM6DSL_H

// =============================================================================
// mcu1_master/sensors/lsm6dsl.h — LSM6DSL 6-axis IMU
// U15, soldered on the main board. SPI1: GPIO20 MOSI, GPIO23 MISO,
// GPIO22 SCK, GPIO21 CS. Mode 3 (CPOL=1, CPHA=1).
//
// Read command is 0x80 | address. WHO_AM_I at 0x0F returns 0x6A.
//
// Also a useful diagnostic: this chip is on the same SPI1 bus as the AS5047
// but soldered to the main board rather than a daughter board. If it reads,
// SPI1 works and the AS5047 failure is the cable or sensor board.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>

#define LSM_REG_WHO_AM_I    0x0F
#define LSM_WHO_AM_I_VAL    0x6A
#define LSM_REG_CTRL1_XL    0x10
#define LSM_REG_CTRL2_G     0x11
#define LSM_REG_OUTX_L_G    0x22    // gyro X low byte, auto-increments

typedef enum {
    LSM_OK          = 0,
    LSM_ERR_SPI     = 1,
    LSM_ERR_WHOAMI  = 2,
} lsm_result_t;

typedef struct { int16_t x, y, z; } lsm_xyz_t;

lsm_result_t lsm6dsl_init(void);
lsm_result_t lsm6dsl_read_gyro(lsm_xyz_t *out);
uint8_t      lsm6dsl_last_whoami(void);

#endif // LSM6DSL_H
