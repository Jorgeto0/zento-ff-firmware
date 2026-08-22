// =============================================================================
// mcu1_master/pio_bus/pio_master.c — PIO Bus Master Driver
// Loads bus_master.pio into PIO0, configures state machines, sends/receives
//
// SM0 = TX (MCU1 → MCU2) — master drives CLK
// SM1 = RX (MCU2 → MCU1) — slave drives CLK
//
// ⚠️  PIN CONSTRAINT: CLK_RX pin must = PIO_BUS_DATA_RX_PIN + 1
//     Tell client before schematic is finalized
// ⚠️  All pins 0xFF until schematic arrives — init returns early safely
// =============================================================================

#include "pio_master.h"
#include "config.h"
#include "crc.h"
#include "diagnostics/uart_log.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/time.h"

// Auto-generated from bus_master.pio by pico_generate_pio_header()
#include "bus_master.pio.h"

// -----------------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------------
static PIO  pio_instance   = pio0;   // We use PIO0 on MCU1
static uint sm_tx          = 0;      // State machine 0 = TX
static uint sm_rx          = 1;      // State machine 1 = RX
static uint offset_tx      = 0;      // PIO memory offset for TX program
static uint offset_rx      = 0;      // PIO memory offset for RX program
static bool bus_ready      = false;  // Set true after successful init

// -----------------------------------------------------------------------------
// PIO clock divider for 1MHz bus
// System clock = 125MHz
// Each bit takes 2 PIO cycles (CLK LOW + CLK HIGH)
// So PIO must run at 2MHz to achieve 1MHz bit rate
// Divider = 125,000,000 / 2,000,000 = 62.5
// -----------------------------------------------------------------------------
#define PIO_CLOCK_DIV       62.5f

// -----------------------------------------------------------------------------
// Receive timeout default
// -----------------------------------------------------------------------------
#define DEFAULT_TIMEOUT_US  5000    // 5ms — plenty for a 26-byte packet

// -----------------------------------------------------------------------------
// pio_master_init()
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// wait_tx_drained()
// pio_sm_put_blocking() only queues into the FIFO — the state machine is still
// shifting bits out when it returns. Switching direction here would truncate
// the packet. Wait for the FIFO to empty, then for the last byte to clock out.
// 8 bits x 2 PIO cycles at 2MHz = 8us; 20us is a safe margin.
// -----------------------------------------------------------------------------
static void wait_tx_drained(void) {
    while (!pio_sm_is_tx_fifo_empty(pio_instance, sm_tx)) {
        tight_loop_contents();
    }
    busy_wait_us(20);
}

pio_bus_result_t pio_master_init(void) {

    // V1 pin roles — see bus_master.pio
    //   PIO1    = data master -> slave  (OUT base for TX)
    //   PIO2    = data slave -> master  (IN base for RX)
    //   PIO_CLK = clock, master-driven in BOTH directions
    const uint pin_tx  = MCU1_PIO1_PIN;
    const uint pin_rx  = MCU1_PIO2_PIN;
    const uint pin_clk = MCU1_PIO_CLK_PIN;

    // ---- TX state machine (SM0) ----
    offset_tx = pio_add_program(pio_instance, &bus_master_tx_program);
    pio_sm_config tx_cfg = bus_master_tx_program_get_default_config(offset_tx);

    sm_config_set_out_pins(&tx_cfg, pin_tx, 1);
    sm_config_set_sideset_pins(&tx_cfg, pin_clk);
    sm_config_set_out_shift(&tx_cfg, false, true, 8);   // MSB first, autopull
    sm_config_set_clkdiv(&tx_cfg, PIO_CLOCK_DIV);

    pio_gpio_init(pio_instance, pin_tx);
    pio_gpio_init(pio_instance, pin_clk);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_tx, pin_tx,  1, true);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_tx, pin_clk, 1, true);

    pio_sm_init(pio_instance, sm_tx, offset_tx, &tx_cfg);

    // ---- RX state machine (SM1) ----
    offset_rx = pio_add_program(pio_instance, &bus_master_rx_program);
    pio_sm_config rx_cfg = bus_master_rx_program_get_default_config(offset_rx);

    sm_config_set_in_pins(&rx_cfg, pin_rx);
    sm_config_set_sideset_pins(&rx_cfg, pin_clk);       // master still clocks
    sm_config_set_in_shift(&rx_cfg, false, true, 8);    // MSB first, autopush
    sm_config_set_clkdiv(&rx_cfg, PIO_CLOCK_DIV);

    pio_gpio_init(pio_instance, pin_rx);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_rx, pin_rx,  1, false);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_rx, pin_clk, 1, true);

    pio_sm_init(pio_instance, sm_rx, offset_rx, &rx_cfg);

    // Half-duplex: exactly one SM runs at a time, since both drive the clock.
    // send() and receive() enable and disable them around each transfer.
    pio_sm_set_enabled(pio_instance, sm_tx, false);
    pio_sm_set_enabled(pio_instance, sm_rx, false);

    bus_ready = true;
    log_info("PIO master init OK — half-duplex, master-driven clock");
    return PIO_BUS_OK;
}

// -----------------------------------------------------------------------------
// pio_master_send()
// Send a complete proto_m2s_t packet to MCU2
// Fills start byte, packet type, length, timestamp, computes CRC
// Then pushes every byte into TX FIFO
// -----------------------------------------------------------------------------
pio_bus_result_t pio_master_send(proto_m2s_t *packet) {
    if (!bus_ready) {
        return PIO_BUS_ERR_PIN_TBD;
    }

    // Fill header fields
    packet->start_byte  = PROTO_START_BYTE;
    packet->packet_type = PROTO_TYPE_M2S;
    packet->length      = sizeof(proto_m2s_t) - 4; // Exclude header + CRC

    // Fill timestamp
    packet->timestamp_us = (uint32_t)time_us_32();

    // Clear reserved bytes
    for (uint8_t i = 0; i < PROTO_RESERVED_BYTES; i++) {
        packet->reserved[i] = 0x00;
    }

    // Compute CRC over everything after start_byte, before crc8 field
    packet->crc8 = crc8_compute(
        (const uint8_t *)packet + 1,        // Skip start_byte
        sizeof(proto_m2s_t) - 2            // Exclude start_byte and crc8
    );

    // Half-duplex: only the TX machine may touch the clock during a send
    pio_sm_set_enabled(pio_instance, sm_rx, false);
    pio_sm_set_enabled(pio_instance, sm_tx, true);

    const uint8_t *bytes = (const uint8_t *)packet;
    for (uint8_t i = 0; i < sizeof(proto_m2s_t); i++) {
        // pio_sm_put_blocking waits if FIFO full — safe, won't drop bytes
        pio_sm_put_blocking(pio_instance, sm_tx, (uint32_t)bytes[i] << 24);
    }

    return PIO_BUS_OK;
}

// -----------------------------------------------------------------------------
// pio_master_receive()
// Receive a proto_s2m_t packet from MCU2
// Validates start byte, packet type, and CRC8
// Blocks until full packet received or timeout
// -----------------------------------------------------------------------------
pio_bus_result_t pio_master_receive(proto_s2m_t *packet, uint32_t timeout_us) {
    if (!bus_ready) {
        return PIO_BUS_ERR_PIN_TBD;
    }

    // Half-duplex: let TX finish shifting before taking the clock away
    wait_tx_drained();
    pio_sm_set_enabled(pio_instance, sm_tx, false);
    pio_sm_clear_fifos(pio_instance, sm_rx);
    pio_sm_set_enabled(pio_instance, sm_rx, true);

    uint8_t *bytes = (uint8_t *)packet;
    uint32_t start = time_us_32();

    for (uint8_t i = 0; i < sizeof(proto_s2m_t); i++) {

        // Wait for byte in RX FIFO with timeout
        while (pio_sm_is_rx_fifo_empty(pio_instance, sm_rx)) {
            if ((time_us_32() - start) > timeout_us) {
                log_error("PIO master RX timeout");
                return PIO_BUS_ERR_TIMEOUT;
            }
        }

        // Read byte from RX FIFO
        // Shift right 24 because PIO pushes into MSB of 32-bit word
        bytes[i] = (uint8_t)(pio_sm_get(pio_instance, sm_rx) >> 24);
    }

    // Validate start byte
    if (packet->start_byte != PROTO_START_BYTE) {
        log_error("PIO master RX bad start byte");
        return PIO_BUS_ERR_BAD_PACKET;
    }

    // Validate packet type
    if (packet->packet_type != PROTO_TYPE_S2M) {
        log_error("PIO master RX wrong packet type");
        return PIO_BUS_ERR_BAD_PACKET;
    }

    // Validate CRC
    if (!crc8_verify((const uint8_t *)packet + 1, sizeof(proto_s2m_t) - 1)) {
        log_error("PIO master RX CRC fail");
        return PIO_BUS_ERR_CRC;
    }

    return PIO_BUS_OK;
}

// -----------------------------------------------------------------------------
// pio_master_is_ready()
// -----------------------------------------------------------------------------
bool pio_master_is_ready(void) {
    return bus_ready;
}
