#ifndef PROTOCOL_H
#define PROTOCOL_H

// =============================================================================
// shared/protocol.h — PIO Bus Packet Definitions
// Used by both MCU1 (master) and MCU2 (slave)
// Physical pins: TBD — defined in config.h when schematic arrives
// =============================================================================
// Packet structure (both directions):
// [START_BYTE][PACKET_TYPE][LENGTH][PAYLOAD][RESERVED][CRC8]
//
// START_BYTE  — always 0xAA — marks beginning of packet
// PACKET_TYPE — identifies direction and content
// LENGTH      — payload length in bytes (not counting header or CRC)
// PAYLOAD     — actual data (fixed size per packet type)
// RESERVED    — future expansion bytes (always send as 0x00)
// CRC8        — checksum over everything except START_BYTE
// =============================================================================

#include <stdint.h>

// -----------------------------------------------------------------------------
// Constants
// -----------------------------------------------------------------------------
#define PROTO_START_BYTE        0xAA  // All packets begin with this

#define PROTO_TYPE_M2S          0x01  // Master to Slave (MCU1 → MCU2)
#define PROTO_TYPE_S2M          0x02  // Slave to Master (MCU2 → MCU1)

#define PROTO_RESERVED_BYTES    4     // Reserved bytes for future expansion

// -----------------------------------------------------------------------------
// Status flags — used in MCU2 → MCU1 packet
// Packed into a single byte — each bit is one flag
// -----------------------------------------------------------------------------
#define STATUS_OK               0x00  // All good
#define STATUS_FAULT_COIL1      (1 << 0)  // Coil 1 fault detected
#define STATUS_FAULT_COIL2      (1 << 1)  // Coil 2 fault detected
#define STATUS_FAULT_COIL3      (1 << 2)  // Coil 3 fault detected
#define STATUS_FAULT_COIL4      (1 << 3)  // Coil 4 fault detected
#define STATUS_FAULT_COIL5      (1 << 4)  // Coil 5 fault detected
#define STATUS_WATCHDOG_WARN    (1 << 5)  // Watchdog getting close to trigger
#define STATUS_TEMP_WARN        (1 << 6)  // Temperature warning
#define STATUS_RESERVED         (1 << 7)  // Reserved — always 0

// -----------------------------------------------------------------------------
// MCU1 → MCU2 Packet (Master to Slave)
// MCU1 sends force commands for Stick 2 coils + sync timestamp
//
// Total size: 1 + 1 + 1 + 14 + 4 + 1 = 22 bytes
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  start_byte;        // Always 0xAA
    uint8_t  packet_type;       // Always PROTO_TYPE_M2S (0x01)
    uint8_t  length;            // Payload length = 14

    // Payload — force commands for Stick 2 (5 coils)
    // Signed 16-bit: negative = reverse direction
    // Range: -32768 to +32767 maps to full reverse to full forward
    int16_t  coil_target[5];    // Target current for coils 1-5 on MCU2

    // Sync timestamp — MCU1 system time in microseconds
    // MCU2 uses this to detect missed packets
    uint32_t timestamp_us;

    // Reserved — send as 0x00, reserved for future use
    uint8_t  reserved[PROTO_RESERVED_BYTES];

    uint8_t  crc8;              // CRC8 over everything after start_byte
} proto_m2s_t;

// -----------------------------------------------------------------------------
// MCU2 → MCU1 Packet (Slave to Master)
// MCU2 sends Stick 2 position + current readings + status
//
// Total size: 1 + 1 + 1 + 18 + 4 + 1 = 26 bytes
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  start_byte;        // Always 0xAA
    uint8_t  packet_type;       // Always PROTO_TYPE_S2M (0x02)
    uint8_t  length;            // Payload length = 18

    // Payload — Stick 2 position from TMAG5273
    // Raw 16-bit values from sensor — scaling happens on MCU1
    int16_t  stick_x;           // Stick 2 X axis
    int16_t  stick_y;           // Stick 2 Y axis
    int16_t  stick_z;           // Stick 2 Z axis (twist or tilt)

    // Current readings for Stick 2 coils (5 coils)
    // Raw 16-bit ADC values — scaling happens on MCU1
    uint16_t coil_current[5];   // Measured current coils 1-5 on MCU2

    // Status flags — see STATUS_* defines above
    uint8_t  status;

    // Reserved — send as 0x00, reserved for future use
    uint8_t  reserved[PROTO_RESERVED_BYTES];

    uint8_t  crc8;              // CRC8 over everything after start_byte
} proto_s2m_t;

// -----------------------------------------------------------------------------
// Packet size sanity checks — caught at compile time
// If these fail the build breaks immediately with a
