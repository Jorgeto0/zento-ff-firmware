// =============================================================================
// mcu1_master/sensors/spi_pio_m1.c — PIO SPI mode 1 implementation
// =============================================================================

#include "spi_pio_m1.h"
#include "spi_pio_m1.pio.h"
#include "hardware/clocks.h"

bool spi_m1_init(spi_m1_t *bus, PIO pio, uint sm,
                 uint pin_mosi, uint pin_miso, uint pin_sck,
                 uint32_t clk_hz) {

    // RP2350B: each PIO block sees only 32 GPIOs at a time. Base 16 gives
    // this block GPIO16-47, covering the sensors (16-23) and the coil bus
    // (38-40). Datasheet GPIOBASE register: only 0 and 16 are supported.
    if (pio_set_gpio_base(pio, 16) != PICO_OK) {
        bus->ready = false;
        return false;
    }

    if (!pio_can_add_program(pio, &spi_mode1_program)) {
        bus->ready = false;
        return false;
    }

    bus->pio    = pio;
    bus->sm     = sm;
    bus->offset = pio_add_program(pio, &spi_mode1_program);

    pio_sm_config c = spi_mode1_program_get_default_config(bus->offset);

    sm_config_set_out_pins(&c, pin_mosi, 1);
    sm_config_set_in_pins(&c, pin_miso);
    sm_config_set_sideset_pins(&c, pin_sck);

    // MSB first. Autopull and autopush both OFF — the program has explicit
    // pull and push instructions, and datasheet 11.5.4 warns that mixing the
    // two changes PULL into a no-op barrier when the OSR is already full.
    sm_config_set_out_shift(&c, false, false, 32);
    sm_config_set_in_shift(&c, false, false, 32);

    // Two PIO instructions per SPI bit, so the state machine runs at 2x
    float div = (float)clock_get_hz(clk_sys) / (float)(clk_hz * 2u);
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(pio, pin_mosi);
    pio_gpio_init(pio, pin_miso);
    pio_gpio_init(pio, pin_sck);

    pio_sm_set_consecutive_pindirs(pio, sm, pin_mosi, 1, true);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_miso, 1, false);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_sck,  1, true);

    pio_sm_init(pio, sm, bus->offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    bus->ready = true;
    return true;
}

uint32_t spi_m1_xfer(spi_m1_t *bus, uint32_t tx, uint8_t bits) {
    if (!bus->ready || bits == 0 || bits > 32) {
        return 0;
    }

    pio_sm_clear_fifos(bus->pio, bus->sm);

    // The program expects two words: the bit count minus one, then the data
    // left-aligned so the first bit out of the OSR is the frame's MSB.
    pio_sm_put_blocking(bus->pio, bus->sm, (uint32_t)(bits - 1));
    pio_sm_put_blocking(bus->pio, bus->sm,
                        (bits == 32) ? tx : (tx << (32 - bits)));

    uint32_t in = pio_sm_get_blocking(bus->pio, bus->sm);

    // The ISR shifts left, so the received bits sit in the low end after
    // exactly 'bits' shifts
    return in & ((bits == 32) ? 0xFFFFFFFFu : ((1u << bits) - 1u));
}
