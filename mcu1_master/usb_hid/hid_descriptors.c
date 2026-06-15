// =============================================================================
// mcu1_master/usb_hid/hid_descriptors.c — USB HID Descriptor Tables
// TinyUSB reads these at enumeration time to tell the PC what we are
// =============================================================================

#include "hid_descriptors.h"
#include "tusb.h"

// -----------------------------------------------------------------------------
// Device Descriptor
// The first thing the PC reads when we plug in
// Tells PC: USB version, class, VID, PID, manufacturer, product strings
// -----------------------------------------------------------------------------
const tusb_desc_device_t usb_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,

    // Class 0 at device level — class defined at interface level
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,   // Device version 1.0

    // Indices into usb_string_descriptor array below
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01      // One configuration
};

// -----------------------------------------------------------------------------
// HID Report Descriptor
// Tells the PC exactly what data we send and receive
// Built using TinyUSB HID descriptor macros
//
// Primary report (ID 1):
//   - 4 axes: Stick1 X/Y, Stick2 X/Y — signed 16-bit each
//   - 5 buttons — 1 bit each, 3 bits padding
//   - 10 coil currents — unsigned 16-bit each (telemetry)
//
// Config report (ID 2):
//   - 63 bytes raw IN/OUT (command + payload)
// -----------------------------------------------------------------------------
const uint8_t usb_hid_report_descriptor[] = {

    // ----- Primary Report (ID 1) — Joystick axes + buttons + telemetry -----
    HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP     ),
    HID_USAGE       ( HID_USAGE_DESKTOP_JOYSTICK ),
    HID_COLLECTION  ( HID_COLLECTION_APPLICATION ),

        HID_REPORT_ID   ( REPORT_ID_PRIMARY         )

        // Stick 1 X axis — signed 16-bit
        HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP    ),
        HID_USAGE       ( HID_USAGE_DESKTOP_X       ),
        HID_LOGICAL_MIN ( 0x8000                    ),  // -32768
        HID_LOGICAL_MAX ( 0x7FFF                    ),  // +32767
        HID_REPORT_COUNT( 1                         ),
        HID_REPORT_SIZE ( 16                        ),
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // Stick 1 Y axis — signed 16-bit
        HID_USAGE       ( HID_USAGE_DESKTOP_Y       ),
        HID_LOGICAL_MIN ( 0x8000                    ),
        HID_LOGICAL_MAX ( 0x7FFF                    ),
        HID_REPORT_COUNT( 1                         ),
        HID_REPORT_SIZE ( 16                        ),
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // Stick 2 X axis — signed 16-bit
        HID_USAGE       ( HID_USAGE_DESKTOP_RX      ),
        HID_LOGICAL_MIN ( 0x8000                    ),
        HID_LOGICAL_MAX ( 0x7FFF                    ),
        HID_REPORT_COUNT( 1                         ),
        HID_REPORT_SIZE ( 16                        ),
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // Stick 2 Y axis — signed 16-bit
        HID_USAGE       ( HID_USAGE_DESKTOP_RY      ),
        HID_LOGICAL_MIN ( 0x8000                    ),
        HID_LOGICAL_MAX ( 0x7FFF                    ),
        HID_REPORT_COUNT( 1                         ),
        HID_REPORT_SIZE ( 16                        ),
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // 5 buttons — 1 bit each
        HID_USAGE_PAGE  ( HID_USAGE_PAGE_BUTTON     ),
        HID_USAGE_MIN   ( 1                         ),
        HID_USAGE_MAX   ( 5                         ),
        HID_LOGICAL_MIN ( 0                         ),
        HID_LOGICAL_MAX ( 1                         ),
        HID_REPORT_COUNT( 5                         ),
        HID_REPORT_SIZE ( 1                         ),
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // 3 bits padding to complete the byte
        HID_REPORT_COUNT( 3                         ),
        HID_REPORT_SIZE ( 1                         ),
        HID_INPUT       ( HID_CONSTANT              ),

        // 10 coil current readings — unsigned 16-bit each
        HID_USAGE_PAGE  ( HID_USAGE_PAGE_ORDINAL    ),
        HID_USAGE_MIN   ( 1                         ),
        HID_USAGE_MAX   ( 10                        ),
        HID_LOGICAL_MIN ( 0                         ),
        HID_LOGICAL_MAX ( 0xFFFF                    ),
        HID_REPORT_COUNT( 10                        ),
        HID_REPORT_SIZE ( 16                        ),
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

    HID_COLLECTION_END,

    // ----- Config Report (ID 2) — Raw 63-byte IN/OUT -----
    HID_USAGE_PAGE  ( HID_USAGE_PAGE_VENDOR ),
    HID_USAGE       ( 0x01                           ),
    HID_COLLECTION  ( HID_COLLECTION_APPLICATION     ),

        HID_REPORT_ID   ( REPORT_ID_CONFIG            )

        // 63 bytes OUT (PC → device): command + payload
        HID_USAGE       ( 0x01                        ),
        HID_LOGICAL_MIN ( 0                           ),
        HID_LOGICAL_MAX ( 0xFF                        ),
        HID_REPORT_COUNT( 63                          ),
        HID_REPORT_SIZE ( 8                           ),
        HID_OUTPUT      ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // 63 bytes IN (device → PC): response + status
        HID_USAGE       ( 0x01                        ),
        HID_REPORT_COUNT( 63                          ),
        HID_REPORT_SIZE ( 8                           ),
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

    HID_COLLECTION_END
};

// -----------------------------------------------------------------------------
// String Descriptors
// Index 0: Language (English US)
// Index 1: Manufacturer
// Index 2: Product name
// Index 3: Serial number
// -----------------------------------------------------------------------------
const uint16_t usb_string_descriptor[][32] = {
    // Index 0 — Language: English US
    { 0x0409 },

    // Index 1 — Manufacturer
    u"ZENTO",

    // Index 2 — Product
    u"ZENTO FF Haptic Controller",

    // Index 3 — Serial number
    u"000001"
};

// -----------------------------------------------------------------------------
// Configuration Descriptor
// Tells PC: power requirements, interfaces, endpoints
// -----------------------------------------------------------------------------

// Total length of config descriptor + all interface + endpoint descriptors
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

// Endpoint numbers
#define EP_HID_PRIMARY      0x81    // IN endpoint 1 (device → PC)
#define EP_HID_CONFIG_IN    0x82    // IN endpoint 2 (device → PC)
#define EP_HID_CONFIG_OUT   0x02    // OUT endpoint 2 (PC → device)

// Polling interval — 1ms = 1000Hz
#define HID_POLL_INTERVAL   1

const uint8_t usb_config_descriptor[] = {
    // Configuration descriptor header
    TUD_CONFIG_DESCRIPTOR(
        1,                  // Configuration number
        1,                  // Number of interfaces
        0,                  // String index (none)
        CONFIG_TOTAL_LEN,   // Total length
        0x00,               // Attributes (bus powered)
        500                 // Max power: 500mA
    ),

    // HID Interface — bidirectional (primary + config on same interface)
    TUD_HID_INOUT_DESCRIPTOR(
        0,                              // Interface number
        0,                              // String index (none)
        HID_ITF_PROTOCOL_NONE,          // Protocol: none (raw HID)
        sizeof(usb_hid_report_descriptor),
        EP_HID_CONFIG_OUT,              // OUT endpoint
        EP_HID_PRIMARY,                 // IN endpoint
        CFG_TUD_HID_EP_BUFSIZE,         // Endpoint buffer size
        HID_POLL_INTERVAL               // 1ms polling
    )
};
