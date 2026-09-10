// =============================================================================
// mcu1_master/sensors/spi_pio.c — PIO SPI master implementation
// =============================================================================

#include "spi_pio.h"
#include "spi_pio.pio.h"
#include "hardware/clocks.h"

bool spi_pio_init(spi_pio_t *bus, PIO pio, uint sm,
                  uint pin_mosi, uint pin_miso, uint pin_sck,
                  uint32_t clk_hz, uint8_t bits) {

    // RP2350B: each PIO block sees only 32 GPIOs at a time. Base 16 gives
    // this block GPIO16-47, covering the sensors (16-23) and the coil bus
    // (38-40). Datasheet GPIOBASE register: only 0 and 16 are supported.
    // pio_set_gpio_base fails with PICO_ERROR_INVALID_STATE once any program
    // is loaded into the block (see pio.c: it checks _used_instruction_space).
    // Several buses share pio1, so only the first one can set it. Skip if it
    // is already correct rather than treating that as a failure.
    if (pio_get_gpio_base(pio) != 16 && pio_set_gpio_base(pio, 16) != PICO_OK) {
        bus->ready = false;
        return false;
    }

    if (!pio_can_add_program(pio, &spi_mode0_program)) {
        bus->ready = false;
        return false;
    }

    bus->pio      = pio;
    bus->sm       = sm;
    bus->offset   = pio_add_program(pio, &spi_mode0_program);
    bus->pin_mosi = pin_mosi;
    bus->pin_miso = pin_miso;
    bus->pin_sck  = pin_sck;
    bus->bits     = bits;

    pio_sm_config c = spi_mode0_program_get_default_config(bus->offset);

    sm_config_set_out_pins(&c, pin_mosi, 1);
    sm_config_set_in_pins(&c, pin_miso);
    sm_config_set_sideset_pins(&c, pin_sck);

    // MSB first, autopull and autopush ON at the frame width. Datasheet
    // 11.5.4: without them the OSR is never refilled and the ISR is never
    // emptied, so nothing is sent and a blocking get would hang forever.
    sm_config_set_out_shift(&c, false, true, bits);
    sm_config_set_in_shift(&c, false, true, bits);

    // Two PIO instructions per SPI bit, so run the state machine at 2x
    float div = (float)clock_get_hz(clk_sys) / (float)(clk_hz * 2u);
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(pio, pin_mosi);
    pio_gpio_init(pio, pin_miso);
    pio_gpio_init(pio, pin_sck);

    pio_sm_set_consecutive_pindirs(pio, sm, pin_mosi, 1, true);   // output
    pio_sm_set_consecutive_pindirs(pio, sm, pin_miso, 1, false);  // input
    pio_sm_set_consecutive_pindirs(pio, sm, pin_sck,  1, true);   // output

    // Returns an error if the config's pins are unreachable from this
    // block's GPIO base. Ignoring it leaves the SM silently misconfigured,
    // which looks exactly like dead hardware.
    if (pio_sm_init(pio, sm, bus->offset, &c) != PICO_OK) {
        bus->ready = false;
        return false;
    }
    pio_sm_set_enabled(pio, sm, true);

    bus->ready = true;
    return true;
}

uint32_t spi_pio_xfer(spi_pio_t *bus, uint32_t tx) {
    uint8_t bits = bus->bits;
    if (!bus->ready || bits == 0 || bits > 32) {
        return 0;
    }

    // Left-align so the first bit out of the OSR is the MSB of the frame
    uint32_t out = (bits == 32) ? tx : (tx << (32 - bits));

    pio_sm_clear_fifos(bus->pio, bus->sm);
    pio_sm_put_blocking(bus->pio, bus->sm, out);

    uint32_t in = pio_sm_get_blocking(bus->pio, bus->sm);

    // The ISR shifts left, so received bits sit at the top
    return (bits == 32) ? in : (in >> (32 - bits));
}
