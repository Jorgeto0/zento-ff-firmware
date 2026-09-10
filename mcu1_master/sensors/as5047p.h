#ifndef AS5047P_H
#define AS5047P_H

// =============================================================================
// mcu1_master/sensors/as5047p.h — AS5047P angle sensor
// Part: AS5047P-ATSM on the AS sensor daughter board, via connector CN9
// Verified against ams datasheet DS000324 v2-00 (2021-Jul-22)
// =============================================================================
// SPI mode 1 (CPOL=0, CPHA=1), 16-bit frames, MSB first, max 10 MHz.
//
// Command frame:  [15] PARC even parity over bits 14:0
//                 [14] R/W, 1 = read
//                 [13:0] address
// Read frame:     [15] PARD, [14] EF error flag, [13:0] data
//
// Reads are pipelined. Datasheet page 13: "The SPI read is sampled on the
// rising edge of CSn and the data is transmitted on MISO with the next read
// command." So one register read takes two frames: send the address, then
// send a NOP to clock the answer out.
//
// Pin directions come from the daughter board: U1 pin 3 MISO is an output and
// pin 4 MOSI is an input (Figure 4), so M_SPI1_SDI is the MCU's MOSI and
// M_SPI1_SDO is the MCU's MISO.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>

// Volatile registers, Figure 18
#define AS_REG_NOP          0x0000
#define AS_REG_ERRFL        0x0001
#define AS_REG_PROG         0x0003
#define AS_REG_DIAAGC       0x3FFC
#define AS_REG_MAG          0x3FFD
#define AS_REG_ANGLEUNC     0x3FFE
#define AS_REG_ANGLECOM     0x3FFF

// DIAAGC bit positions, Figure 21
#define AS_DIAAGC_AGC_MASK  0x00FF   // bits 7:0
#define AS_DIAAGC_LF        (1u << 8)   // offset compensation finished
#define AS_DIAAGC_COF       (1u << 9)   // CORDIC overflow, angle unreliable
#define AS_DIAAGC_MAGH      (1u << 10)  // field too strong, AGC = 0x00
#define AS_DIAAGC_MAGL      (1u << 11)  // field too weak,   AGC = 0xFF

typedef enum {
    AS_OK           = 0,
    AS_ERR_SPI      = 1,   // bus not initialised
    AS_ERR_PARITY   = 2,   // reply parity failed
    AS_ERR_FLAG     = 3,   // EF set, sensor rejected the previous command
    AS_ERR_DEAD     = 4,   // all-zero or all-one frame, bus floating
} as_result_t;

typedef struct {
    uint8_t  agc;      // 0x00 field too strong, 0xFF too weak
    bool     magh;
    bool     magl;
    bool     cof;
    bool     lf;
} as_diag_t;

as_result_t as5047_init(void);

// Read one register. Costs two SPI frames because reads are pipelined.
as_result_t as5047_read_reg(uint16_t addr, uint16_t *out);

// 14-bit angle with dynamic angle error compensation, 0 to 16383
as_result_t as5047_read_angle(uint16_t *out);

// Diagnostics. AGC and the field-strength flags are what indicate magnetic
// interference from the coils or the other stick's magnet.
as_result_t as5047_read_diag(as_diag_t *out);

// Convert a 14-bit reading to degrees
float as5047_to_degrees(uint16_t raw);

#endif // AS5047P_H
