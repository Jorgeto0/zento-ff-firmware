#ifndef TUSB_CONFIG_H
#define TUSB_CONFIG_H

// =============================================================================
// mcu1_master/usb_hid/tusb_config.h — TinyUSB Configuration
// HID only — no CDC, no MSC, no other USB classes
// TinyUSB requires this file to be findable in include path
// =============================================================================

// -----------------------------------------------------------------------------
// Board and controller
// -----------------------------------------------------------------------------
#define CFG_TUSB_MCU                OPT_MCU_RP2040
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE

// -----------------------------------------------------------------------------
// OS — no RTOS, bare metal
// -----------------------------------------------------------------------------
#define CFG_TUSB_OS                 OPT_OS_NONE

// -----------------------------------------------------------------------------
// Debug — set to 0 for production, 1 for TinyUSB internal logging
// -----------------------------------------------------------------------------
#define CFG_TUSB_DEBUG              0

// -----------------------------------------------------------------------------
// HID — one interface, bidirectional
// -----------------------------------------------------------------------------
#define CFG_TUD_HID                 1
#define CFG_TUD_HID_EP_BUFSIZE      64

// -----------------------------------------------------------------------------
// All other classes disabled
// -----------------------------------------------------------------------------
#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

// -----------------------------------------------------------------------------
// Endpoint 0 buffer size
// -----------------------------------------------------------------------------
#define CFG_TUD_ENDPOINT0_SIZE      64

#endif // TUSB_CONFIG_H
