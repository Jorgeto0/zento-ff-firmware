// =============================================================================
// mcu2_slave/pio_bus/pio_slave.c — PIO Bus Slave Driver
// Loads bus_slave.pio into PIO0, configures state machines, receives/sends
//
// SM0 = RX (MCU2 receives from MCU1) — master drives CLK
// SM1 = TX (MCU2 sends to MCU1)     — slave drives CLK
//
// ⚠️  PIN CONSTRAINT: CLK_RX pin must = MCU2_PIO_BUS_PIN_0 + 1
// ⚠️  All pins 0xFF until schematic arrives — init returns early safely
// =============================================================================

#include "pio_slave.h"
#include "config.h"
#include "crc.h"
#include "diagnostics/uart_log.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/time.h"

// Auto-generated from bus_slave.pio by pico_generate_pio_header()
#include "bus_slave.pio.h"

// -----------------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------------
static PIO  pio_instance   = pio0;   // Use PIO0 on MCU2
static uint sm_rx          = 0;      // State machine 0 = RX
static uint sm_tx          = 1;      // State machine 1 = TX
static uint offset_rx      = 0;      // PIO memory offset for RX program
static uint offset_tx      = 0;      // PIO memory offset for TX program
static bool bus_ready      = false;  // Set true after successful init

// -----------------------------------------------------------------------------
// PIO clock divider — matches master exactly
// System clock 125MHz, 2 PIO cycles per bit, target 1MHz bus
// Divider = 125,000,000 / 2,000,000 = 62.5
// -----------------------------------------------------------------------------
// Slave runs 4x faster than the master. The master holds PIO_CLK high for a
// single PIO cycle, and the slave's bit loop is 4 instructions against the
// master's 3 — at matched rates the slave drifts and misses clock edges.
// Verified in simulation: matched rates lose the data, 4x recovers it exactly.
#define PIO_CLOCK_DIV       15.625f

// -----------------------------------------------------------------------------
// pio_slave_init()
// -----------------------------------------------------------------------------
pio_slave_result_t pio_slave_init(void) {

    // V1 pin roles — see bus_slave.pio
    //   PIO1    = data master -> slave  (IN base for RX)
    //   PIO2    = data slave -> master  (OUT base for TX)
    //   PIO_CLK = clock, INPUT only on this side — slave never drives it
    //
    // Both programs use IN base = PIO1 so that 'wait pin 4' lands on
    // PIO1 + 4 = PIO_CLK. This depends on GPIO1-5 being contiguous.
    const uint pin_rx  = MCU2_PIO1_PIN;
    const uint pin_tx  = MCU2_PIO2_PIN;
    const uint pin_clk = MCU2_PIO_CLK_PIN;

    // ---- RX state machine (SM0) ----
    offset_rx = pio_add_program(pio_instance, &bus_slave_rx_program);
    pio_sm_config rx_cfg = bus_slave_rx_program_get_default_config(offset_rx);

    sm_config_set_in_pins(&rx_cfg, pin_rx);
    sm_config_set_in_shift(&rx_cfg, false, true, 8);    // MSB first, autopush
    sm_config_set_clkdiv(&rx_cfg, PIO_CLOCK_DIV);

    pio_gpio_init(pio_instance, pin_rx);
    pio_gpio_init(pio_instance, pin_clk);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_rx, pin_rx,  1, false);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_rx, pin_clk, 1, false);

    pio_sm_init(pio_instance, sm_rx, offset_rx, &rx_cfg);

    // ---- TX state machine (SM1) ----
    offset_tx = pio_add_program(pio_instance, &bus_slave_tx_program);
    pio_sm_config tx_cfg = bus_slave_tx_program_get_default_config(offset_tx);

    sm_config_set_out_pins(&tx_cfg, pin_tx, 1);
    sm_config_set_in_pins(&tx_cfg, pin_rx);             // for the wait offset
    sm_config_set_out_shift(&tx_cfg, false, true, 8);   // MSB first, autopull
    sm_config_set_clkdiv(&tx_cfg, PIO_CLOCK_DIV);

    pio_gpio_init(pio_instance, pin_tx);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_tx, pin_tx, 1, true);

    pio_sm_init(pio_instance, sm_tx, offset_tx, &tx_cfg);

    // RX idles enabled — the slave must be listening whenever the master
    // decides to talk. TX is enabled only while replying.
    // Both enabled permanently — see note in pio_slave_send()
    pio_sm_set_enabled(pio_instance, sm_rx, true);
    pio_sm_set_enabled(pio_instance, sm_tx, true);

    bus_ready = true;
    log_info("PIO slave init OK — listening, clock is master-driven");
    return PIO_SLAVE_OK;
}

// -----------------------------------------------------------------------------
// pio_slave_receive()
// Receive a proto_m2s_t packet from MCU1
// Validates start byte, packet type, CRC8
// -----------------------------------------------------------------------------
pio_slave_result_t pio_slave_receive(proto_m2s_t *packet, uint32_t timeout_us) {
    if (!bus_ready) {
        return PIO_SLAVE_ERR_PIN_TBD;
    }

    uint8_t *bytes = (uint8_t *)packet;
    uint32_t start = time_us_32();

    for (uint8_t i = 0; i < sizeof(proto_m2s_t); i++) {

        // Wait for byte in RX FIFO with timeout
        while (pio_sm_is_rx_fifo_empty(pio_instance, sm_rx)) {
            if ((time_us_32() - start) > timeout_us) {
                log_error("PIO slave RX timeout");
                return PIO_SLAVE_ERR_TIMEOUT;
            }
        }

        // Read byte — shift right 24, PIO pushes into MSB of 32-bit word
        bytes[i] = (uint8_t)(pio_sm_get(pio_instance, sm_rx) >> 24);
    }

    // Validate start byte
    if (packet->start_byte != PROTO_START_BYTE) {
        log_error("PIO slave RX bad start byte");
        return PIO_SLAVE_ERR_BAD_PACKET;
    }

    // Validate packet type
    if (packet->packet_type != PROTO_TYPE_M2S) {
        log_error("PIO slave RX wrong packet type");
        return PIO_SLAVE_ERR_BAD_PACKET;
    }

    // Validate CRC
    if (!crc8_verify((const uint8_t *)packet + 1, sizeof(proto_m2s_t) - 1)) {
        log_error("PIO slave RX CRC fail");
        return PIO_SLAVE_ERR_CRC;
    }

    return PIO_SLAVE_OK;
}

// -----------------------------------------------------------------------------
// pio_slave_send()
// Send a proto_s2m_t packet to MCU1
// Fills header, timestamp, computes CRC, pushes to TX FIFO
// -----------------------------------------------------------------------------
pio_slave_result_t pio_slave_send(proto_s2m_t *packet) {
    if (!bus_ready) {
        return PIO_SLAVE_ERR_PIN_TBD;
    }

    // Fill header fields
    packet->start_byte  = PROTO_START_BYTE;
    packet->packet_type = PROTO_TYPE_S2M;
    packet->length      = sizeof(proto_s2m_t) - 4;

    // Clear reserved bytes
    for (uint8_t i = 0; i < PROTO_RESERVED_BYTES; i++) {
        packet->reserved[i] = 0x00;
    }

    // Compute CRC
    packet->crc8 = crc8_compute(
        (const uint8_t *)packet + 1,
        sizeof(proto_s2m_t) - 2
    );

    // Both slave SMs stay enabled — no gating needed here. Neither drives
    // the clock, so they cannot conflict, and 'pull block' stalls TX
    // whenever its FIFO is empty. Disabling RX during a send would drop
    // the first byte of the master's next packet.
    const uint8_t *bytes = (const uint8_t *)packet;
    for (uint8_t i = 0; i < sizeof(proto_s2m_t); i++) {
        pio_sm_put_blocking(pio_instance, sm_tx, (uint32_t)bytes[i] << 24);
    }

    return PIO_SLAVE_OK;
}

// -----------------------------------------------------------------------------
// pio_slave_is_ready()
// -----------------------------------------------------------------------------
bool pio_slave_is_ready(void) {
    return bus_ready;
}
