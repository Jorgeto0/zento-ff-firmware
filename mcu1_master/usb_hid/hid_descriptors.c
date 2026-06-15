// =============================================================================
// mcu1_master/usb_hid/hid_descriptors.c — USB HID Descriptor Tables
// TinyUSB reads these at enumeration time to tell the PC what we are
// =============================================================================

#include "hid_descriptors.h"
#include "tusb.h"

// -----------------------------------------------------------------------------
// Device Descriptor
// -----------------------------------------------------------------------------
const tusb_desc_device_t usb_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

// -----------------------------------------------------------------------------
// HID Report Descriptor
// Uses HID_LOGICAL_MIN_N / HID_LOGICAL_MAX_N for 16-bit values
// -----------------------------------------------------------------------------
const uint8_t usb_hid_report_descriptor[] = {

    // ----- Primary Report (ID 1) -----
    HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP     ),
    HID_USAGE       ( HID_USAGE_DESKTOP_JOYSTICK ),
    HID_COLLECTION  ( HID_COLLECTION_APPLICATION ),

        HID_REPORT_ID ( REPORT_ID_PRIMARY         )

        // Stick 1 X
        HID_USAGE           ( HID_USAGE_DESKTOP_X   ),
        HID_LOGICAL_MIN_N   ( -32768, 2             ),
        HID_LOGICAL_MAX_N   ( 32767,  2             ),
        HID_REPORT_COUNT    ( 1                     ),
        HID_REPORT_SIZE     ( 16                    ),
        HID_INPUT           ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // Stick 1 Y
        HID_USAGE           ( HID_USAGE_DESKTOP_Y   ),
        HID_LOGICAL_MIN_N   ( -32768, 2             ),
        HID_LOGICAL_MAX_N   ( 32767,  2             ),
        HID_REPORT_COUNT    ( 1                     ),
        HID_REPORT_SIZE     ( 16                    ),
        HID_INPUT           ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // Stick 2 X
        HID_USAGE           ( HID_USAGE_DESKTOP_RX  ),
        HID_LOGICAL_MIN_N   ( -32768, 2             ),
        HID_LOGICAL_MAX_N   ( 32767,  2             ),
        HID_REPORT_COUNT    ( 1                     ),
        HID_REPORT_SIZE     ( 16                    ),
        HID_INPUT           ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // Stick 2 Y
        HID_USAGE           ( HID_USAGE_DESKTOP_RY  ),
        HID_LOGICAL_MIN_N   ( -32768, 2             ),
        HID_LOGICAL_MAX_N   ( 32767,  2             ),
        HID_REPORT_COUNT    ( 1                     ),
        HID_REPORT_SIZE     ( 16                    ),
        HID_INPUT           ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // 5 buttons
        HID_USAGE_PAGE      ( HID_USAGE_PAGE_BUTTON ),
        HID_USAGE_MIN       ( 1                     ),
        HID_USAGE_MAX       ( 5                     ),
        HID_LOGICAL_MIN     ( 0                     ),
        HID_LOGICAL_MAX     ( 1                     ),
        HID_REPORT_COUNT    ( 5                     ),
        HID_REPORT_SIZE     ( 1                     ),
        HID_INPUT           ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // 3 bits padding
        HID_REPORT_COUNT    ( 3                     ),
        HID_REPORT_SIZE     ( 1                     ),
        HID_INPUT           ( HID_CONSTANT          ),

        // 10 coil currents unsigned 16-bit
        HID_USAGE_PAGE      ( HID_USAGE_PAGE_DESKTOP ),
        HID_USAGE           ( HID_USAGE_DESKTOP_X   ),
        HID_LOGICAL_MIN_N   ( 0,     2              ),
        HID_LOGICAL_MAX_N   ( 65535, 2              ),
        HID_REPORT_COUNT    ( 10                    ),
        HID_REPORT_SIZE     ( 16                    ),
        HID_INPUT           ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

    HID_COLLECTION_END,

    // ----- Config Report (ID 2) -----
    HID_USAGE_PAGE  ( HID_USAGE_PAGE_DESKTOP      ),
    HID_USAGE       ( HID_USAGE_DESKTOP_GAMEPAD   ),
    HID_COLLECTION  ( HID_COLLECTION_APPLICATION  ),

        HID_REPORT_ID ( REPORT_ID_CONFIG           )

        // 63 bytes OUT
        HID_USAGE           ( HID_USAGE_DESKTOP_X  ),
        HID_LOGICAL_MIN     ( 0                    ),
        HID_LOGICAL_MAX_N   ( 255, 2               ),
        HID_REPORT_COUNT    ( 63                   ),
        HID_REPORT_SIZE     ( 8                    ),
        HID_OUTPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

        // 63 bytes IN
        HID_USAGE           ( HID_USAGE_DESKTOP_X  ),
        HID_REPORT_COUNT    ( 63                   ),
        HID_REPORT_SIZE     ( 8                    ),
        HID_INPUT           ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ),

    HID_COLLECTION_END
};

// -----------------------------------------------------------------------------
// String Descriptors
// -----------------------------------------------------------------------------
const uint16_t usb_string_descriptor[][32] = {
    { 0x0409 },
    u"ZENTO",
    u"ZENTO FF Haptic Controller",
    u"000001"
};

// -----------------------------------------------------------------------------
// Configuration Descriptor
// -----------------------------------------------------------------------------
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)
#define EP_HID_PRIMARY      0x81
#define EP_HID_CONFIG_IN    0x82
#define EP_HID_CONFIG_OUT   0x02
#define HID_POLL_INTERVAL   1

const uint8_t usb_config_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(
        1, 1, 0, CONFIG_TOTAL_LEN, 0x00, 500
    ),
    TUD_HID_INOUT_DESCRIPTOR(
        0, 0,
        HID_ITF_PROTOCOL_NONE,
        sizeof(usb_hid_report_descriptor),
        EP_HID_CONFIG_OUT,
        EP_HID_PRIMARY,
        CFG_TUD_HID_EP_BUFSIZE,
        HID_POLL_INTERVAL
    )
};
