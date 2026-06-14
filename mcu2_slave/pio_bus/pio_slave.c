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
#define PIO_CLOCK_DIV       62.5f

// -----------------------------------------------------------------------------
// pio_slave_init()
// -----------------------------------------------------------------------------
pio_slave_result_t pio_slave_init(void) {

    // Guard — skip if pins not assigned yet
    if (MCU2_PIO_BUS_PIN_0 == 0xFF || MCU2_PIO_BUS_PIN_1 == 0xFF ||
        MCU2_PIO_BUS_PIN_2 == 0xFF || MCU2_PIO_BUS_PIN_3 == 0xFF) {
        log_warning("PIO slave init skipped — pins TBD in config.h");
        return PIO_SLAVE_ERR_PIN_TBD;
    }

    // Pin assignments from config.h
    // Convention (must match master side):
    //   MCU2_PIO_BUS_PIN_0 = DATA_RX (MCU1 → MCU2)
    //   MCU2_PIO_BUS_PIN_1 = CLK_RX  (master drives) — must = PIN_0 + 1
    //   MCU2_PIO_BUS_PIN_2 = DATA_TX (MCU2 → MCU1)
    //   MCU2_PIO_BUS_PIN_3 = CLK_TX  (slave drives during TX)
    uint pin_data_rx = MCU2_PIO_BUS_PIN_0;
    uint pin_clk_rx  = MCU2_PIO_BUS_PIN_1;  // Must = DATA_RX + 1
    uint pin_data_tx = MCU2_PIO_BUS_PIN_2;
    uint pin_clk_tx  = MCU2_PIO_BUS_PIN_3;

    // -------------------------------------------------------------------------
    // Load RX program (SM0)
    // -------------------------------------------------------------------------
    offset_rx = pio_add_program(pio_instance, &bus_slave_rx_program);

    pio_sm_config rx_config = bus_slave_rx_program_get_default_config(offset_rx);

    // IN pin base = DATA_RX
    // CLK_RX = DATA_RX + 1 (PIO constraint)
    sm_config_set_in_pins(&rx_config, pin_data_rx);

    // Shift in MSB first, auto-push at 8 bits
    sm_config_set_in_shift(&rx_config, false, true, 8);

    // Clock divider — must match master
    sm_config_set_clkdiv(&rx_config, PIO_CLOCK_DIV);

    // GPIO directions — RX pins are inputs
    pio_gpio_init(pio_instance, pin_data_rx);
    pio_gpio_init(pio_instance, pin_clk_rx);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_rx, pin_data_rx, 1, false);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_rx, pin_clk_rx, 1, false);

    // Apply config and enable SM0
    pio_sm_init(pio_instance, sm_rx, offset_rx, &rx_config);
    pio_sm_set_enabled(pio_instance, sm_rx, true);

    // -------------------------------------------------------------------------
    // Load TX program (SM1)
    // -------------------------------------------------------------------------
    offset_tx = pio_add_program(pio_instance, &bus_slave_tx_program);

    pio_sm_config tx_config = bus_slave_tx_program_get_default_config(offset_tx);

    // OUT pin = DATA_TX
    sm_config_set_out_pins(&tx_config, pin_data_tx, 1);

    // SIDESET pin = CLK_TX
    sm_config_set_sideset_pins(&tx_config, pin_clk_tx);

    // Shift out MSB first, auto-pull at 8 bits
    sm_config_set_out_shift(&tx_config, false, true, 8);

    // Clock divider — must match master
    sm_config_set_clkdiv(&tx_config, PIO_CLOCK_DIV);

    // GPIO directions — TX pins are outputs
    pio_gpio_init(pio_instance, pin_data_tx);
    pio_gpio_init(pio_instance, pin_clk_tx);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_tx, pin_data_tx, 1, true);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_tx, pin_clk_tx, 1, true);

    // Apply config and enable SM1
    pio_sm_init(pio_instance, sm_tx, offset_tx, &tx_config);
    pio_sm_set_enabled(pio_instance, sm_tx, true);

    bus_ready = true;
    log_info("PIO slave init OK — bus ready at 1MHz");

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

    // Fill timestamp
    packet->timestamp_us = (uint32_t)time_us_32();

    // Clear reserved bytes
    for (uint8_t i = 0; i < PROTO_RESERVED_BYTES; i++) {
        packet->reserved[i] = 0x00;
    }

    // Compute CRC
    packet->crc8 = crc8_compute(
        (const uint8_t *)packet + 1,
        sizeof(proto_s2m_t) - 2
    );

    // Push every byte into TX FIFO
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
