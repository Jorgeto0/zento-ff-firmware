// =============================================================================
// mcu2_slave/main.c — MCU2 Slave Boot Sequence
// Target: RP2350B — Slave role
// Clock: 125MHz (matches MCU1 exactly — required for PIO bus timing)
// Watchdog: 500ms timeout
// Phase 1 skeleton — all TBD pins skipped safely
// ⚠️  All MCU2 pins are 0xFF until updated schematic arrives
// =============================================================================

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "config.h"
#include "diagnostics/uart_log.h"

// -----------------------------------------------------------------------------
// System clock — must match MCU1 exactly
// PIO bus timing depends on both chips running same clock
// -----------------------------------------------------------------------------
#define SYS_CLOCK_KHZ       125000

// -----------------------------------------------------------------------------
// Watchdog timeout — matches MCU1
// -----------------------------------------------------------------------------
#define WATCHDOG_TIMEOUT_MS 500

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
static void system_clock_init(void);
static void watchdog_init(void);
static void gpio_init_all(void);
static void check_reset_reason(void);

// -----------------------------------------------------------------------------
// main()
// Boot order matches MCU1 exactly — same reasoning applies
// 1. Clock first
// 2. UART next
// 3. Check reset reason
// 4. GPIO init
// 5. Watchdog last
// -----------------------------------------------------------------------------
int main(void) {

    // Step 1 — Set system clock
    system_clock_init();

    // Step 2 — UART logging
    // ⚠️  MCU2 UART pins are 0xFF — uart_log_init() skips safely
    uart_log_init();
    log_info("MCU2 boot started");
    log_value("SYS_CLOCK_KHZ", SYS_CLOCK_KHZ);

    // Step 3 — Check reset reason
    check_reset_reason();

    // Step 4 — Initialize all known GPIOs to safe states
    gpio_init_all();
    log_info("GPIO init complete");

    // Step 5 — Start watchdog
    watchdog_init();
    log_info("Watchdog started — 500ms timeout");

    // Boot complete
    log_info("MCU2 slave ready — awaiting PIO bus sync from MCU1");

    // -------------------------------------------------------------------------
    // Main loop
    // MCU2 is slave — it waits for commands from MCU1 via PIO bus
    // PIO bus driver added in next step
    // -------------------------------------------------------------------------
    while (1) {

        // Feed watchdog — must happen every loop iteration
        watchdog_update();

        // Phase 1 placeholder — PIO slave driver added in next step
        // DO NOT add blocking calls here
        // DO NOT add sleep_ms() here

    }

    return 0;
}

// -----------------------------------------------------------------------------
// system_clock_init()
// ⚠️  Must match MCU1 exactly — PIO bus timing depends on this
// -----------------------------------------------------------------------------
static void system_clock_init(void) {
    bool exact = set_sys_clock_khz(SYS_CLOCK_KHZ, false);
    stdio_init_all();
    (void)exact;
}

// -----------------------------------------------------------------------------
// watchdog_init()
// -----------------------------------------------------------------------------
static void watchdog_init(void) {
    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
}

// -----------------------------------------------------------------------------
// gpio_init_all()
// All MCU2 pins currently 0xFF — all skipped safely
// When schematic arrives and config.h is updated, this runs automatically
// -----------------------------------------------------------------------------
static void gpio_init_all(void) {

    
// MCU2 motor PWM pins — all 0xFF until schematic arrives

    const uint8_t pwm_pins[] = {
    PWM_COIL5_PIN, PWM_COIL6_PIN, PWM_COIL7_PIN,
    PWM_COIL8_PIN, PWM_COIL9_PIN
    };
    
    for (uint8_t i = 0; i < 5; i++) {
        if (pwm_pins[i] != 0xFF) {
            gpio_init(pwm_pins[i]);
            gpio_set_dir(pwm_pins[i], GPIO_OUT);
            gpio_put(pwm_pins[i], 0);
        }
    }

    // MCU2 DIR pins

    const uint8_t dir_pins[] = {
    DIR_COIL5_PIN, DIR_COIL6_PIN, DIR_COIL7_PIN,
    DIR_COIL8_PIN, DIR_COIL9_PIN
    };

    for (uint8_t i = 0; i < 5; i++) {
        if (dir_pins[i] != 0xFF) {
            gpio_init(dir_pins[i]);
            gpio_set_dir(dir_pins[i], GPIO_OUT);
            gpio_put(dir_pins[i], 0);
        }
    }

    // MCU2 EN pin
    const uint8_t en_pins[] = {
        EN_COIL_COIL_RIGHT_PIN,
        EN_COIL_THUMB_RIGHT_PIN
    };

    for (uint8_t i = 0; i < 2; i++) {
        gpio_init(en_pins[i]);
        gpio_set_dir(en_pins[i], GPIO_OUT);
        gpio_put(en_pins[i], 0);
    }
    // MCU2 I2C pins — direction set by I2C driver, not here
    // Listed for reference — initialized when I2C driver comes in Phase 2
}

// -----------------------------------------------------------------------------
// check_reset_reason()
// -----------------------------------------------------------------------------
static void check_reset_reason(void) {
    if (watchdog_caused_reboot()) {
        log_error("RESET CAUSE: Watchdog timeout — main loop stalled");
    } else {
        log_info("RESET CAUSE: Normal power-on or manual reset");
    }
}
