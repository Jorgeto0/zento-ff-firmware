#ifndef HID_H
#define HID_H

// =============================================================================
// mcu1_master/usb_hid/hid.h — USB HID Runtime Interface
// Handles sending reports to PC and receiving config packets from PC
// Runs on MCU1 only — MCU2 never talks to USB directly
// =============================================================================

#include <stdint.h>
#include <stdbool.h>
#include "hid_descriptors.h"

// -----------------------------------------------------------------------------
// hid_init()
// Initialize TinyUSB stack
// Must be called once at boot after system clock is stable
// -----------------------------------------------------------------------------
void hid_init(void);

// -----------------------------------------------------------------------------
// hid_task()
// Must be called every main loop iteration
// Drives TinyUSB internal state machine
// Never block inside this — it will break 1000Hz timing
// -----------------------------------------------------------------------------
void hid_task(void);

// -----------------------------------------------------------------------------
// hid_send_primary()
// Send primary report to PC (axes + buttons + currents)
// Call at 1000Hz from main loop
//
// report  — pointer to filled hid_primary_report_t
// returns — true if sent successfully, false if USB not ready
// -----------------------------------------------------------------------------
bool hid_send_primary(hid_primary_report_t *report);

// -----------------------------------------------------------------------------
// hid_send_config()
// Send config response back to PC
// Call after processing a config command from PC
//
// report  — pointer to filled hid_config_report_t
// returns — true if sent, false if USB not ready
// -----------------------------------------------------------------------------
bool hid_send_config(hid_config_report_t *report);

// -----------------------------------------------------------------------------
// hid_is_connected()
// Returns true if PC has enumerated us and is actively polling
// Check this before sending reports — avoids wasted cycles
// -----------------------------------------------------------------------------
bool hid_is_connected(void);

// -----------------------------------------------------------------------------
// hid_get_config_received()
// Returns true if a new config packet arrived from PC
// Clears the flag after returning true — call once per loop
// -----------------------------------------------------------------------------
bool hid_get_config_received(void);

// -----------------------------------------------------------------------------
// hid_get_last_config()
// Get the last config packet received from PC
// Only valid if hid_get_config_received() returned true
// -----------------------------------------------------------------------------
void hid_get_last_config(hid_config_report_t *out);

#endif // HID_H
