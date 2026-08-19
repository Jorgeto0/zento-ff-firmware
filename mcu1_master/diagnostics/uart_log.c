// =============================================================================
// mcu1_master/diagnostics/uart_log.c — UART Diagnostic Logging Implementation
// Zero allocation — no printf, no malloc, no heap usage ever
// Output format: [INFO] message\r\n
// =============================================================================

#include "uart_log.h"
#include "config.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include <stdint.h>
#include <stddef.h>

// -----------------------------------------------------------------------------
// Internal constants
// -----------------------------------------------------------------------------
#define LOG_UART        uart0
#define LOG_BAUD_RATE   115200

// -----------------------------------------------------------------------------
// Internal helpers — no dynamic allocation, no printf
// -----------------------------------------------------------------------------

// Write a null-terminated string to UART
static void uart_write_str(const char *str) {
    while (*str) {
        uart_putc_raw(LOG_UART, *str++);
    }
}

// Write a single character to UART
static void uart_write_char(char c) {
    uart_putc_raw(LOG_UART, c);
}

// Convert uint32_t to decimal string — writes directly to UART
// No buffer needed — digits written right to left then reversed
static void uart_write_uint(uint32_t value) {
    char buf[10];  // Max 10 digits for uint32
    uint8_t i = 0;

    if (value == 0) {
        uart_write_char('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    // Digits are backwards — write in reverse
    while (i > 0) {
        uart_write_char(buf[--i]);
    }
}

// Convert int32_t to decimal string — handles negative values
static void uart_write_int(int32_t value) {
    if (value < 0) {
        uart_write_char('-');
        // Cast to uint32 after negation — avoids overflow on INT32_MIN
        uart_write_uint((uint32_t)(-(value + 1)) + 1);
    } else {
        uart_write_uint((uint32_t)value);
    }
}

// Convert uint32_t to hex string — always writes 8 hex digits
static void uart_write_hex(uint32_t value) {
    static const char hex_chars[] = "0123456789ABCDEF";
    uart_write_str("0x");
    for (int8_t i = 28; i >= 0; i -= 4) {
        uart_write_char(hex_chars[(value >> i) & 0xF]);
    }
}

// Write end of line
static void uart_write_eol(void) {
    uart_write_str("\r\n");
}

// -----------------------------------------------------------------------------
// uart_log_init()
// Call once at boot before any logging
// -----------------------------------------------------------------------------
void uart_log_init(void) {
    // Skip UART init if pins not yet assigned
    // v2 schematic: GPIO0 is SW1 — no free UART pins on MCU1
    if (UART0_TX_PIN == 0xFF || UART0_RX_PIN == 0xFF) {
        return;
    }


    // Initialize UART0 at 115200 baud
    uart_init(LOG_UART, LOG_BAUD_RATE);

    // Set GPIO function to UART
    gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);

    // Disable hardware flow control — not needed for diagnostics
    uart_set_hw_flow(LOG_UART, false, false);

    // 8 data bits, 1 stop bit, no parity — standard serial
    uart_set_format(LOG_UART, 8, 1, UART_PARITY_NONE);

    // Send boot marker — makes it easy to spot resets in terminal
    uart_write_str("\r\n=== MCU1 UART LOG INIT ===\r\n");
}

// -----------------------------------------------------------------------------
// log_info()
// -----------------------------------------------------------------------------
void log_info(const char *msg) {
    if (UART0_TX_PIN == 0xFF) return;

    uart_write_str("[INFO] ");
    uart_write_str(msg);
    uart_write_eol();
}

// -----------------------------------------------------------------------------
// log_warning()
// -----------------------------------------------------------------------------
void log_warning(const char *msg) {
    if (UART0_TX_PIN == 0xFF) return;

    uart_write_str("[WARN] ");
    uart_write_str(msg);
    uart_write_eol();
}

// -----------------------------------------------------------------------------
// log_error()
// -----------------------------------------------------------------------------
void log_error(const char *msg) {
    if (UART0_TX_PIN == 0xFF) return;

    uart_write_str("[ERR]  ");
    uart_write_str(msg);
    uart_write_eol();
}

// -----------------------------------------------------------------------------
// log_value()
// Output format: [VAL]  label = 1234
// -----------------------------------------------------------------------------
void log_value(const char *label, int32_t value) {
    if (UART0_TX_PIN == 0xFF) return;

    uart_write_str("[VAL]  ");
    uart_write_str(label);
    uart_write_str(" = ");
    uart_write_int(value);
    uart_write_eol();
}

// -----------------------------------------------------------------------------
// log_hex()
// Output format: [HEX]  label = 0x0000AABB
// -----------------------------------------------------------------------------
void log_hex(const char *label, uint32_t value) {
    if (UART0_TX_PIN == 0xFF) return;

    uart_write_str("[HEX]  ");
    uart_write_str(label);
    uart_write_str(" = ");
    uart_write_hex(value);
    uart_write_eol();
}
