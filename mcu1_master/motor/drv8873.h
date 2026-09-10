#ifndef DRV8873_H
#define DRV8873_H

// =============================================================================
// mcu1_master/motor/drv8873.h — DRV8873S H-bridge driver
// Five per side: coils 1-4 plus one voice coil, sharing one SPI bus with an
// individual chip select each.
// Verified against TI datasheet SLVSET1
// =============================================================================
// SPI mode 1 (CPOL=0, CPHA=1) per 7.5.1: SCLK must be low when nSCS goes low,
// data is captured on the falling edge and driven on the rising edge.
// 16-bit frames, max 10 MHz, nSCS high >= 500 ns between frames.
//
// Frame:  [15]=0  [14]=W (1=read)  [13:9]=address  [8]=X  [7:0]=data
// Reply:  [15:8]= 1,1,OTW,UVLO,CPUV,OCP,TSD,OLD    [7:0]=register data
//
// Bus directions traced through the netlist: M_SPI_DI_COIL reaches the
// driver's SDI input so it is the MCU's MOSI, M_SPI_DO_COIL comes from its
// SDO output so it is the MCU's MISO.
//
// IMPORTANT: MODE in IC1[1:0] defaults to 01b (PWM mode) but this board wires
// M_PWM_COILx to EN/IN1 and M_DIR_COILx to PH/IN2, which is PH/EN mode = 00b.
// It must be written at init or the pins will not behave as the schematic
// intends. See Table 2 and Table 4.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>

// Register map, 7.6
#define DRV_REG_FAULT       0x00    // read only
#define DRV_REG_DIAG        0x01    // read only
#define DRV_REG_IC1         0x02
#define DRV_REG_IC2         0x03
#define DRV_REG_IC3         0x04
#define DRV_REG_IC4         0x05

// IC1 fields
#define DRV_MODE_PH_EN      0x0     // bits 1:0 — what this board is wired for
#define DRV_MODE_PWM        0x1     // power-up default
#define DRV_SR_SHIFT        2
#define DRV_SPI_IN          (1u<<5)
#define DRV_TOFF_SHIFT      6

// IC3 fields
#define DRV_CLR_FLT         (1u<<7)
#define DRV_LOCK_SHIFT      4
#define DRV_LOCK_UNLOCK     0x4     // 100b unlocks IC1
#define DRV_LOCK_LOCK       0x3     // 011b locks it

// Status byte in every reply, 7.5.1.2
#define DRV_ST_OTW          (1u<<5)
#define DRV_ST_UVLO         (1u<<4)
#define DRV_ST_CPUV         (1u<<3)
#define DRV_ST_OCP          (1u<<2)
#define DRV_ST_TSD          (1u<<1)
#define DRV_ST_OLD          (1u<<0)

typedef enum {
    DRV_OK          = 0,
    DRV_ERR_SPI     = 1,
    DRV_ERR_DEAD    = 2,    // no reply, bus floating or driver absent
    DRV_ERR_FAULT   = 3,    // device reports a fault
} drv_result_t;

typedef enum {
    DRV_COIL1 = 0, DRV_COIL2, DRV_COIL3, DRV_COIL4, DRV_VC1,
    DRV_COUNT
} drv_id_t;

// Bitmask of drivers that answered, bit 0 = COIL1. Only two coils are wired
// today, so the rest are expected to be absent.
uint8_t      drv_present_mask(void);

drv_result_t drv_init_all(void);
drv_result_t drv_read_reg(drv_id_t id, uint8_t addr, uint8_t *out, uint8_t *status);
drv_result_t drv_write_reg(drv_id_t id, uint8_t addr, uint8_t value);
drv_result_t drv_read_fault(drv_id_t id, uint8_t *fault, uint8_t *diag);

#endif // DRV8873_H
