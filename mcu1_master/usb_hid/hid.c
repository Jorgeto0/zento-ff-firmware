// =============================================================================
// mcu1_master/usb_hid/hid.c — USB HID Runtime Implementation
// TinyUSB stack driver — handles enumeration, reports, config packets
// 1000Hz primary report rate target
// =============================================================================

#include "hid.h"
#include "hid_descriptors.h"
#include "diagnostics/uart_log.h"
#include "tusb.h"

// -----------------------------------------------------------------------------
// Internal state
// -----------------------------------------------------------------------------
static bool config_received       = false;
static hid_config_report_t last_config;

// -----------------------------------------------------------------------------
// hid_init()
// Initialize TinyUSB — must be called after system clock is stable
// -----------------------------------------------------------------------------
void hid_init(void) {
    tusb_init();
    log_info("USB HID init — TinyUSB started");
}

// -----------------------------------------------------------------------------
// hid_task()
// Drive TinyUSB internal state machine
// Must be called every main loop iteration — never skip
// -----------------------------------------------------------------------------
void hid_task(void) {
    tud_task();
}

// -----------------------------------------------------------------------------
// hid_send_primary()
// Send primary report to PC
// Only sends if PC is connected and endpoint is ready
// -----------------------------------------------------------------------------
bool hid_send_primary(hid_primary_report_t *report) {
    if (!tud_hid_ready()) {
        return false;
    }

    report->report_id = REPORT_ID_PRIMARY;

    return tud_hid_report(
        REPORT_ID_PRIMARY,
        report,
        sizeof(hid_primary_report_t)
    );
}

// -----------------------------------------------------------------------------
// hid_send_config()
// Send config response to PC
// -----------------------------------------------------------------------------
bool hid_send_config(hid_config_report_t *report) {
    if (!tud_hid_ready()) {
        return false;
    }

    report->report_id = REPORT_ID_CONFIG;

    return tud_hid_report(
        REPORT_ID_CONFIG,
        report,
        sizeof(hid_config_report_t)
    );
}

// -----------------------------------------------------------------------------
// hid_is_connected()
// -----------------------------------------------------------------------------
bool hid_is_connected(void) {
    return tud_connected() && tud_hid_ready();
}

// -----------------------------------------------------------------------------
// hid_get_config_received()
// Returns true once when new config packet arrives, then clears flag
// -----------------------------------------------------------------------------
bool hid_get_config_received(void) {
    if (config_received) {
        config_received = false;
        return true;
    }
    return false;
}

// -----------------------------------------------------------------------------
// hid_get_last_config()
// Copy last received config packet to caller's buffer
// -----------------------------------------------------------------------------
void hid_get_last_config(hid_config_report_t *out) {
    *out = last_config;
}

// =============================================================================
// TinyUSB Callbacks — called automatically by TinyUSB internals
// These MUST exist — TinyUSB will not link without them
// =============================================================================

// -----------------------------------------------------------------------------
// tud_descriptor_device_cb()
// TinyUSB calls this when PC requests device descriptor
// -----------------------------------------------------------------------------
uint8_t const *tud_descriptor_device_cb(void) {
    return (uint8_t const *)&usb_device_descriptor;
}

// -----------------------------------------------------------------------------
// tud_descriptor_configuration_cb()
// TinyUSB calls this when PC requests configuration descriptor
// -----------------------------------------------------------------------------
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return usb_config_descriptor;
}

// -----------------------------------------------------------------------------
// tud_descriptor_string_cb()
// TinyUSB calls this when PC requests string descriptors
// -----------------------------------------------------------------------------
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;

    uint8_t  count;
    static uint16_t string_buf[32];

    if (index == 0) {
        // Language descriptor
        string_buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | 4);
        string_buf[1] = 0x0409;  // English US
        return string_buf;
    }

    if (index >= 4) {
        return NULL;
    }

    // Copy string into buffer with descriptor header
    const uint16_t *str = usb_string_descriptor[index];
    count = 0;
    while (str[count] != 0 && count < 31) count++;

    // Header: length + string descriptor type
    string_buf[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 + count * 2));
    for (uint8_t i = 0; i < count; i++) {
        string_buf[1 + i] = str[i];
    }

    return string_buf;
}

// -----------------------------------------------------------------------------
// tud_hid_descriptor_report_cb()
// TinyUSB calls this when PC requests HID report descriptor
// -----------------------------------------------------------------------------
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return usb_hid_report_descriptor;
}

// -----------------------------------------------------------------------------
// tud_hid_set_report_cb()
// TinyUSB calls this when PC sends us a config packet (OUT report)
// We store it and set flag — main loop picks it up via hid_get_config_received()
// -----------------------------------------------------------------------------
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;
    (void)report_type;

    if (report_id == REPORT_ID_CONFIG &&
        bufsize <= sizeof(hid_config_report_t)) {

        // Copy into last_config buffer
        uint8_t *dst = (uint8_t *)&last_config;
        for (uint16_t i = 0; i < bufsize; i++) {
            dst[i] = buffer[i];
        }

        config_received = true;
        log_info("USB HID config packet received");
    }
}

// -----------------------------------------------------------------------------
// tud_hid_get_report_cb()
// TinyUSB calls this when PC polls for a report
// We handle sending via hid_send_primary() in main loop
// This callback handles any GET_REPORT requests — return empty for now
// -----------------------------------------------------------------------------
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

// -----------------------------------------------------------------------------
// tud_mount_cb() — called when PC successfully enumerates device
// -----------------------------------------------------------------------------
void tud_mount_cb(void) {
    log_info("USB mounted — PC connected");
}

// -----------------------------------------------------------------------------
// tud_umount_cb() — called when PC disconnects
// -----------------------------------------------------------------------------
void tud_umount_cb(void) {
    log_info("USB unmounted — PC disconnected");
}

// -----------------------------------------------------------------------------
// tud_suspend_cb() — called when USB bus suspends
// -----------------------------------------------------------------------------
void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    log_warning("USB suspended");
}

// -----------------------------------------------------------------------------
// tud_resume_cb() — called when USB bus resumes
// -----------------------------------------------------------------------------
void tud_resume_cb(void) {
    log_info("USB resumed");
}
