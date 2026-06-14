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
pio_bus_result_t pio_master_init(void) {

    // Guard — skip if pins not assigned yet
    if (PIO_BUS_PIN_0 == 0xFF || PIO_BUS_PIN_1 == 0xFF ||
        PIO_BUS_PIN_2 == 0xFF || PIO_BUS_PIN_3 == 0xFF) {
        log_warning("PIO master init skipped — pins TBD in config.h");
        return PIO_BUS_ERR_PIN_TBD;
    }

    // Pin assignments from config.h
    // ⚠️  PIO_BUS_PIN_0/1/2/3 must be assigned correctly when schematic arrives
    // Convention we will use (confirm with schematic):
    //   PIO_BUS_PIN_0 = DATA_TX (MCU1 → MCU2)
    //   PIO_BUS_PIN_1 = CLK_TX  (MCU1 drives during TX)
    //   PIO_BUS_PIN_2 = DATA_RX (MCU2 → MCU1)
    //   PIO_BUS_PIN_3 = CLK_RX  (MCU2 drives during its TX)
    //   ⚠️  CLK_RX must = DATA_RX + 1 (PIO constraint)
    uint pin_data_tx = PIO_BUS_PIN_0;
    uint pin_clk_tx  = PIO_BUS_PIN_1;
    uint pin_data_rx = PIO_BUS_PIN_2;
    uint pin_clk_rx  = PIO_BUS_PIN_3;  // Must be DATA_RX + 1

    // -------------------------------------------------------------------------
    // Load TX program (SM0)
    // -------------------------------------------------------------------------
    offset_tx = pio_add_program(pio_instance, &bus_master_tx_program);

    pio_sm_config tx_config = bus_master_tx_program_get_default_config(offset_tx);

    // OUT pin = DATA_TX
    sm_config_set_out_pins(&tx_config, pin_data_tx, 1);

    // SIDESET pin = CLK_TX
    sm_config_set_sideset_pins(&tx_config, pin_clk_tx);

    // Shift out MSB first, auto-pull at 8 bits
    sm_config_set_out_shift(&tx_config, false, true, 8);

    // Set clock divider — 1MHz bus
    sm_config_set_clkdiv(&tx_config, PIO_CLOCK_DIV);

    // Initialize GPIO directions for TX pins
    pio_gpio_init(pio_instance, pin_data_tx);
    pio_gpio_init(pio_instance, pin_clk_tx);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_tx, pin_data_tx, 1, true);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_tx, pin_clk_tx, 1, true);

    // Apply config and enable SM0
    pio_sm_init(pio_instance, sm_tx, offset_tx, &tx_config);
    pio_sm_set_enabled(pio_instance, sm_tx, true);

    // -------------------------------------------------------------------------
    // Load RX program (SM1)
    // -------------------------------------------------------------------------
    offset_rx = pio_add_program(pio_instance, &bus_master_rx_program);

    pio_sm_config rx_config = bus_master_rx_program_get_default_config(offset_rx);

    // IN pin base = DATA_RX
    // CLK_RX = DATA_RX + 1 (PIO constraint — wait pin 1 = base + 1)
    sm_config_set_in_pins(&rx_config, pin_data_rx);

    // Shift in MSB first, auto-push at 8 bits
    sm_config_set_in_shift(&rx_config, false, true, 8);

    // Set clock divider — same as TX
    sm_config_set_clkdiv(&rx_config, PIO_CLOCK_DIV);

    // Initialize GPIO directions for RX pins
    pio_gpio_init(pio_instance, pin_data_rx);
    pio_gpio_init(pio_instance, pin_clk_rx);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_rx, pin_data_rx, 1, false);
    pio_sm_set_consecutive_pindirs(pio_instance, sm_rx, pin_clk_rx, 1, false);

    // Apply config and enable SM1
    pio_sm_init(pio_instance, sm_rx, offset_rx, &rx_config);
    pio_sm_set_enabled(pio_instance, sm_rx, true);

    bus_ready = true;
    log_info("PIO master init OK — bus ready at 1MHz");

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

    // Push every byte into TX FIFO
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
