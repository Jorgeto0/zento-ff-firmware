#ifndef PIO_MASTER_H
#define PIO_MASTER_H

// =============================================================================
// mcu1_master/pio_bus/pio_master.h — PIO Bus Master Driver Interface
// Loads and runs bus_master.pio state machines
// Provides send/receive functions for the rest of MCU1 firmware
// Physical pins: from config.h — all 0xFF until schematic arrives
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "protocol.h"

// -----------------------------------------------------------------------------
// Bus result codes
// Every function returns one of these — never ignore the return value
// -----------------------------------------------------------------------------
typedef enum {
    PIO_BUS_OK              = 0,  // Success
    PIO_BUS_ERR_PIN_TBD     = 1,  // Pins still 0xFF — bus not initialized
    PIO_BUS_ERR_TIMEOUT     = 2,  // No response from MCU2 within timeout
    PIO_BUS_ERR_CRC         = 3,  // Packet received but CRC failed
    PIO_BUS_ERR_BAD_PACKET  = 4,  // Wrong start byte or packet type
    PIO_BUS_ERR_FIFO_FULL   = 5,  // TX FIFO full — MCU2 not reading
} pio_bus_result_t;

// -----------------------------------------------------------------------------
// pio_master_init()
// Load PIO programs, configure state machines, assign pins
// Must be called once before any send/receive
//
// Returns PIO_BUS_ERR_PIN_TBD if pins are still 0xFF in config.h
// Returns PIO_BUS_OK if initialized successfully
// -----------------------------------------------------------------------------
pio_bus_result_t pio_master_init(void);

// -----------------------------------------------------------------------------
// pio_master_send()
// Send a Master→Slave packet to MCU2
// Automatically appends CRC8 before sending
//
// packet  — pointer to filled proto_m2s_t struct
// returns — PIO_BUS_OK on success, error code on failure
// -----------------------------------------------------------------------------
pio_bus_result_t pio_master_send(proto_m2s_t *packet);

// -----------------------------------------------------------------------------
// pio_master_receive()
// Receive a Slave→Master packet from MCU2
// Automatically validates CRC8
// Blocks until packet received or timeout
//
// packet      — pointer to proto_s2m_t struct to fill
// timeout_us  — how long to wait in microseconds before giving up
// returns     — PIO_BUS_OK on success, error code on failure
// -----------------------------------------------------------------------------
pio_bus_result_t pio_master_receive(proto_s2m_t *packet, uint32_t timeout_us);

// -----------------------------------------------------------------------------
// pio_master_is_ready()
// Check if PIO bus was initialized successfully
// Use this before sending — safe guard against TBD pins
//
// returns — true if ready, false if not initialized
// -----------------------------------------------------------------------------
bool pio_master_is_ready(void);

#endif // PIO_MASTER_H
