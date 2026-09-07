#ifndef SPI_PIO_M1_H
#define SPI_PIO_M1_H

// =============================================================================
// mcu1_master/sensors/spi_pio_m1.h — PIO SPI master, mode 1
//
// For the AS5047P. Datasheet DS000324 v2-00: mode 1 (CPOL=0, CPHA=1), MOSI
// sampled by the slave on the falling clock edge, so the master changes data
// on the rising edge and samples on the falling.
//
// Separate from spi_pio.h (mode 0, used by the TMAG5170) because the clock
// must idle LOW here: tL requires CLK low for 350 ns before CS falls, so the
// state machine stalls on a side-0 instruction.
//
// Verified in simulation: MOSI changes on SCK=1, MISO sampled on SCK=0,
// 16 bits recovered exactly, clock idles low while waiting on the FIFO.
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "hardware/pio.h"

typedef struct {
    PIO   pio;
    uint  sm;
    uint  offset;
    bool  ready;
} spi_m1_t;

bool spi_m1_init(spi_m1_t *bus, PIO pio, uint sm,
                 uint pin_mosi, uint pin_miso, uint pin_sck,
                 uint32_t clk_hz);

// Full-duplex transfer, MSB first, 1 to 32 bits. Returns the received word,
// right-aligned. Chip select is the caller's responsibility.
uint32_t spi_m1_xfer(spi_m1_t *bus, uint32_t tx, uint8_t bits);

#endif // SPI_PIO_M1_H
