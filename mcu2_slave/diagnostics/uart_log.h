#ifndef UART_LOG_H
#define UART_LOG_H

// =============================================================================
// mcu2_slave/diagnostics/uart_log.h — UART Diagnostic Logging
// MCU2 only — zero allocation, fixed buffers, severity levels
// Output: UART0 at 115200 baud
// ⚠️  UART pins TBD — defined in config.h when schematic arrives
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
// -----------------------------------------------------------------------------
#define LOG_MAX_MSG_LEN     128

// -----------------------------------------------------------------------------
// Function declarations — identical interface to MCU1
// -----------------------------------------------------------------------------
void uart_log_init(void);
void log_info(const char *msg);
void log_warning(const char *msg);
void log_error(const char *msg);
void log_value(const char *label, int32_t value);
void log_hex(const char *label, uint32_t value);

#endif // UART_LOG_H
