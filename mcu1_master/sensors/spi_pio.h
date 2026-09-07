#ifndef SPI_PIO_H
#define SPI_PIO_H

// =============================================================================
// mcu1_master/sensors/spi_pio.h — PIO-based SPI master
//
// The RP2350 hardware SPI blocks cannot be used on this board: no SPI bus in
// Controller_Board_V1 matches the pin mux. Checked against pico-sdk
// io_bank0.h — on the hall sensor bus GPIO16 is SPI0_RX yet feeds the
// sensor's SDI input, and GPIO19 is SPI0_TX yet reads its SDO output.
// PIO drives any pin in either direction, so the board works as wired.
//
// Chip select is left to the caller. The TMAG5170 needs CS held low across a
// full 32-bit frame (datasheet 7.5.2.2), which a per-byte CS would break.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"

typedef struct {
    PIO      pio;
    uint     sm;
    uint     offset;
    uint     pin_mosi;
    uint     pin_miso;
    uint     pin_sck;
    uint8_t  bits;      // frame width, fixed per bus
    bool     ready;
} spi_pio_t;

// Set up one state machine for a bus.
// clk_hz is the SPI clock; the PIO runs at 2x that, since one bit takes two
// instructions. Returns false if the program will not fit.
bool spi_pio_init(spi_pio_t *bus, PIO pio, uint sm,
                  uint pin_mosi, uint pin_miso, uint pin_sck,
                  uint32_t clk_hz, uint8_t bits);

// Full-duplex transfer, MSB first, 1 to 32 bits.
// Returns what was shifted in, right-aligned.
uint32_t spi_pio_xfer(spi_pio_t *bus, uint32_t tx);

#endif // SPI_PIO_H
