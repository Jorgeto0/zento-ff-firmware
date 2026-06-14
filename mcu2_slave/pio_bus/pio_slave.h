#ifndef PIO_SLAVE_H
#define PIO_SLAVE_H

// =============================================================================
// mcu2_slave/pio_bus/pio_slave.h — PIO Bus Slave Driver Interface
// Loads and runs bus_slave.pio state machines
// Provides receive/send functions for MCU2 firmware
// Physical pins: from config.h — all 0xFF until schematic arrives
//
// ⚠️  PIN CONSTRAINT: CLK_RX pin must = PIO_BUS_DATA_RX_PIN + 1
//     Same constraint as master side — consecutive GPIOs required
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

// -----------------------------------------------------------------------------
// Bus result codes — mirrors master for consistency
// -----------------------------------------------------------------------------
typedef enum {
    PIO_SLAVE_OK              = 0,  // Success
    PIO_SLAVE_ERR_PIN_TBD     = 1,  // Pins still 0xFF — not initialized
    PIO_SLAVE_ERR_TIMEOUT     = 2,  // No packet from MCU1 within timeout
    PIO_SLAVE_ERR_CRC         = 3,  // Packet received but CRC failed
    PIO_SLAVE_ERR_BAD_PACKET  = 4,  // Wrong start byte or packet type
} pio_slave_result_t;

// -----------------------------------------------------------------------------
// pio_slave_init()
// Load PIO programs, configure state machines, assign pins
// Must be called once at boot before any receive/send
//
// Returns PIO_SLAVE_ERR_PIN_TBD if pins still 0xFF
// Returns PIO_SLAVE_OK if initialized successfully
// -----------------------------------------------------------------------------
pio_slave_result_t pio_slave_init(void);

// -----------------------------------------------------------------------------
// pio_slave_receive()
// Receive a Master→Slave packet from MCU1
// Validates start byte, packet type, CRC8
// Blocks until packet received or timeout
//
// packet      — pointer to proto_m2s_t struct to fill
// timeout_us  — microseconds to wait before giving up
// returns     — PIO_SLAVE_OK on success, error code on failure
// -----------------------------------------------------------------------------
pio_slave_result_t pio_slave_receive(proto_m2s_t *packet, uint32_t timeout_us);

// -----------------------------------------------------------------------------
// pio_slave_send()
// Send a Slave→Master packet to MCU1
// Automatically fills header, timestamp, computes CRC8
//
// packet  — pointer to filled proto_s2m_t struct
// returns — PIO_SLAVE_OK on success, error code on failure
// -----------------------------------------------------------------------------
pio_slave_result_t pio_slave_send(proto_s2m_t *packet);

// -----------------------------------------------------------------------------
// pio_slave_is_ready()
// Returns true if PIO bus initialized successfully
// -----------------------------------------------------------------------------
bool pio_slave_is_ready(void);

#endif // PIO_SLAVE_H
