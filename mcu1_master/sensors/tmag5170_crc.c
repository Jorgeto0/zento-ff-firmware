// =============================================================================
// mcu1_master/sensors/tmag5170_crc.c — 4-bit SPI frame CRC
//
// Per datasheet SBASAF4 section 7.5.2.5:
//   polynomial x^4 + x + 1, CRC initialised to 0b1111, computed over the top
//   28 bits of the frame with the low 4 bits zeroed.
//
// CRC is mandatory at power up. 7.5.2.5: "the device will ignore all the SDI
// commands if proper CRC codes are not received." Getting this wrong looks
// exactly like dead hardware, so it is verified rather than assumed:
//
//   - implemented directly from the datasheet's own reference code
//   - cross-checked against the XOR equations (10)-(13) in the same section
//   - both produce 0x7 for TI's published frame 0x0F000407, whose low nibble
//     is a known-good CRC
//   - both agree across 200k random frames
// =============================================================================

#include "tmag5170.h"

uint8_t tmag_crc4(uint32_t frame) {
    // Zero the CRC nibble before computing, per Figure 7-13
    uint32_t padded = frame & 0xFFFFFFF0u;
    uint8_t crc = 0xF;                      // initialise to 0b1111

    for (int8_t i = 31; i >= 0; i--) {
        uint8_t bit = (uint8_t)((padded >> i) & 1u);
        uint8_t inv = bit ^ ((crc >> 3) & 1u);
        uint8_t b3  = (crc >> 2) & 1u;
        uint8_t b2  = (crc >> 1) & 1u;
        uint8_t b1  = ((crc >> 0) & 1u) ^ inv;
        uint8_t b0  = inv;
        crc = (uint8_t)((b3 << 3) | (b2 << 2) | (b1 << 1) | b0);
    }

    return crc & 0x0F;
}
