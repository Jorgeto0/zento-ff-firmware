#ifndef UART_LOG_H
#define UART_LOG_H

// =============================================================================
// mcu1_master/diagnostics/uart_log.h — UART Diagnostic Logging
// MCU1 only — zero allocation, fixed buffers, severity levels
// Output: UART0 at 115200 baud
// ⚠️  GPIO0/1 shared with ESP8684 — safe for Phase 1, revisit in Phase 4
// =============================================================================

#include <stdint.h>

// -----------------------------------------------------------------------------
// Log severity levels
// -----------------------------------------------------------------------------
#define LOG_LEVEL_INFO      0
#define LOG_LEVEL_WARNING   1
#define LOG_LEVEL_ERROR     2

// -----------------------------------------------------------------------------
// Maximum length of a single log message
// Fixed buffer — never changes at runtime
// -----------------------------------------------------------------------------
#define LOG_MAX_MSG_LEN     128

// -----------------------------------------------------------------------------
// uart_log_init()
// Must be called once at boot before any logging
// Sets up UART0 at 115200 baud on GPIO0/GPIO1
// -----------------------------------------------------------------------------
void uart_log_init(void);

// -----------------------------------------------------------------------------
// log_info()
// Log an informational message — normal operation
// Example: log_info("MCU1 boot complete");
// -----------------------------------------------------------------------------
void log_info(const char *msg);

// -----------------------------------------------------------------------------
// log_warning()
// Log a warning — unexpected but recoverable
// Example: log_warning("PIO bus packet CRC mismatch");
// -----------------------------------------------------------------------------
void log_warning(const char *msg);

// -----------------------------------------------------------------------------
// log_error()
// Log an error — something failed
// Example: log_error("Watchdog reset detected");
// -----------------------------------------------------------------------------
void log_error(const char *msg);

// -----------------------------------------------------------------------------
// log_value()
// Log a labeled integer value — for sensor readings, counters, states
// Example: log_value("COIL1_CURRENT", 1024);
// -----------------------------------------------------------------------------
void log_value(const char *label, int32_t value);

// -----------------------------------------------------------------------------
// log_hex()
// Log a labeled value in hex — for packet dumps, register values
// Example: log_hex("CRC8", 0xAF);
// -----------------------------------------------------------------------------
void log_hex(const char *label, uint32_t value);

#endif // UART_LOG_H
