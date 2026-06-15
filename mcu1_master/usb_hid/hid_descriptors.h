#ifndef HID_DESCRIPTORS_H
#define HID_DESCRIPTORS_H

// =============================================================================
// mcu1_master/usb_hid/hid_descriptors.h — USB HID Descriptor Definitions
// Device: ZENTO FF Haptic Controller
// USB Stack: TinyUSB (bundled in Pico SDK)
//
// ⚠️  VID/PID are development placeholders — replace before shipping
//     Ask client if they have official USB-IF VID/PID assigned
// =============================================================================

#include <stdint.h>
#include "tusb.h"

// -----------------------------------------------------------------------------
// USB Device Identity
// ⚠️  0xCAFE = Raspberry Pi dev VID — placeholder only
// ⚠️  0x0001 = placeholder PID
// -----------------------------------------------------------------------------
#define USB_VID             0xCAFE
#define USB_PID             0x0001
#define USB_BCD             0x0200  // USB 2.0

// -----------------------------------------------------------------------------
// HID Report IDs
// Every report must have a unique ID when using multiple endpoints
// -----------------------------------------------------------------------------
#define REPORT_ID_PRIMARY   1   // Axes + buttons + current telemetry
#define REPORT_ID_CONFIG    2   // Config packets (PID params, profiles)

// -----------------------------------------------------------------------------
// Report sizes
// -----------------------------------------------------------------------------
#define REPORT_PRIMARY_SIZE  25  // bytes: 4x axes(8) + buttons(1) + currents(16)
#define REPORT_CONFIG_SIZE   64  // bytes: raw config packet

// -----------------------------------------------------------------------------
// Primary HID report structure
// Sent to PC at 1000Hz
// __attribute__((packed)) — no padding, exact byte layout
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  report_id;         // Always REPORT_ID_PRIMARY (1)

    // Stick positions — signed 16-bit
    // Range: -32768 to +32767
    int16_t  stick1_x;          // Stick 1 X axis
    int16_t  stick1_y;          // Stick 1 Y axis
    int16_t  stick2_x;          // Stick 2 X axis (from MCU2 via PIO bus)
    int16_t  stick2_y;          // Stick 2 Y axis (from MCU2 via PIO bus)

    // Button states — 1 bit per button, packed into 1 byte
    // Bit 0 = SW1, Bit 1 = SW2 ... Bit 4 = SW5, Bits 5-7 reserved
    uint8_t  buttons;

    // Current telemetry — raw ADC counts from all 10 coils
    // PC/SimHub uses these to monitor force feedback intensity
    uint16_t coil_current[10];
} hid_primary_report_t;

// -----------------------------------------------------------------------------
// Config HID report structure
// Bidirectional — PC sends config, we send back status
// -----------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t  report_id;         // Always REPORT_ID_CONFIG (2)
    uint8_t  command;           // What the PC wants to do
    uint8_t  payload[62];       // Command-specific data
} hid_config_report_t;

// -----------------------------------------------------------------------------
// Config commands — PC sends these in hid_config_report_t.command
// -----------------------------------------------------------------------------
#define CONFIG_CMD_SET_PID      0x01  // Set PID parameters for a coil
#define CONFIG_CMD_SET_DEADZONE 0x02  // Set deadzone for a stick axis
#define CONFIG_CMD_SET_PROFILE  0x03  // Load a haptic profile
#define CONFIG_CMD_GET_STATUS   0x04  // Request device status
#define CONFIG_CMD_ACK          0xFF  // Acknowledgement from device

// -----------------------------------------------------------------------------
// External declarations — defined in hid_descriptors.c
// TinyUSB calls these to get descriptor data
// -----------------------------------------------------------------------------
extern const tusb_desc_device_t     usb_device_descriptor;
extern const uint8_t                usb_hid_report_descriptor[];
extern const uint8_t                usb_config_descriptor[];
extern const uint16_t               usb_string_descriptor[][32];

#endif // HID_DESCRIPTORS_H
