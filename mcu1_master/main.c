// =============================================================================
// mcu1_master/main.c — MCU1 Master Boot Sequence
// Target: RP2350B — Master role
// Clock: 125MHz (SDK default — increase later if needed)
// Watchdog: 500ms timeout
// Phase 1 — boot sequence only, no peripheral init yet
// =============================================================================

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "hardware/gpio.h"
#include "config.h"
#include "diagnostics/uart_log.h"

// -----------------------------------------------------------------------------
// System clock frequency
// 125MHz — SDK default, safe and stable on RP2350B
// Change here only — never hardcode MHz anywhere else
// -----------------------------------------------------------------------------
#define SYS_CLOCK_KHZ       125000

// -----------------------------------------------------------------------------
// Watchdog timeout
// 500ms — generous for Phase 1, tighten in later phases
// If main loop doesn't call watchdog_update() within this window — chip resets
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
// Boot order is critical — do not reorder without understanding dependencies
// 1. Clock first — everything else depends on stable clock
// 2. UART next — so we can log everything that follows
// 3. Check reset reason — know why we booted
// 4. GPIO init — safe pin states before any peripheral touches them
// 5. Watchdog last — starts countdown, main loop must keep feeding it
// -----------------------------------------------------------------------------
int main(void) {

    // Step 1 — Set system clock
    system_clock_init();

    // Step 2 — UART logging (needed before anything else so we can debug)
    uart_log_init();
    log_info("MCU1 boot started");
    log_value("SYS_CLOCK_KHZ", SYS_CLOCK_KHZ);

    // Step 3 — Check why we booted (normal power on vs watchdog reset)
    check_reset_reason();

    // Step 4 — Initialize all GPIOs to safe states
    gpio_init_all();
    log_info("GPIO init complete");

    // Step 5 — Start watchdog (must feed it in main loop from this point on)
    watchdog_init();
    log_info("Watchdog started — 500ms timeout");

    // Boot complete
    log_info("MCU1 boot complete — entering main loop");

    // -------------------------------------------------------------------------
    // Main loop
    // Must call watchdog_update() every iteration — never block here
    // All work done via state machines and flags — no blocking calls
    // -------------------------------------------------------------------------
    while (1) {

        // Feed watchdog — must happen every loop iteration
        // If this stops being called — chip resets in 500ms
        watchdog_update();

        // Phase 1 placeholder — peripheral drivers added in later steps
        // DO NOT add blocking calls here
        // DO NOT add sleep_ms() here — use timestamps instead

    }

    // Never reached — MCU runs forever
    return 0;
}

// -----------------------------------------------------------------------------
// system_clock_init()
// Set RP2350B system clock to 125MHz
// Must be called before anything else — UART baud rate depends on clock
// -----------------------------------------------------------------------------
static void system_clock_init(void) {
    // set_sys_clock_khz() adjusts PLL to hit target frequency
    // Returns true if exact frequency achieved, false if approximated
    bool exact = set_sys_clock_khz(SYS_CLOCK_KHZ, false);

    // Reinitialize stdio after clock change — baud rate depends on clock
    stdio_init_all();

    // We log this after UART init — just store result for now
    (void)exact;  // Silence unused variable warning until UART is up
}

// -----------------------------------------------------------------------------
// watchdog_init()
// Enable hardware watchdog with 500ms timeout
// After this call — main loop MUST call watchdog_update() continuously
// -----------------------------------------------------------------------------
static void watchdog_init(void) {
    // pause_on_debug = true — watchdog pauses when debugger halts chip
    // This prevents false resets during SWD debugging sessions
    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);
}

// -----------------------------------------------------------------------------
// gpio_init_all()
// Set all known GPIOs to safe states before peripheral drivers touch them
// All outputs start LOW — no accidental motor enable at boot
// All TBD pins (0xFF) are skipped safely
// -----------------------------------------------------------------------------
static void gpio_init_all(void) {

    // Motor PWM pins — output, start LOW (motors off)
    const uint8_t pwm_pins[] = {
        PWM_COIL1_PIN, PWM_COIL2_PIN, PWM_COIL3_PIN,
        PWM_COIL4_PIN, PWM_COIL5_PIN, PWM_COIL6_PIN,
        PWM_COIL7_PIN, PWM_COIL8_PIN, PWM_COIL9_PIN,
        PWM_COIL10_PIN
    };

    for (uint8_t i = 0; i < 10; i++) {
        if (pwm_pins[i] != 0xFF) {
            gpio_init(pwm_pins[i]);
            gpio_set_dir(pwm_pins[i], GPIO_OUT);
            gpio_put(pwm_pins[i], 0);
        }
    }

    // DIR pins — output, start LOW
    const uint8_t dir_pins[] = {
        DIR_COIL1_PIN, DIR_COIL2_PIN, DIR_COIL3_PIN,
        DIR_COIL4_PIN, DIR_COIL5_PIN, DIR_COIL6_PIN,
        DIR_COIL7_PIN, DIR_COIL8_PIN, DIR_COIL9_PIN,
        DIR_COIL10_PIN
    };

    for (uint8_t i = 0; i < 10; i++) {
        if (dir_pins[i] != 0xFF) {
            gpio_init(dir_pins[i]);
            gpio_set_dir(dir_pins[i], GPIO_OUT);
            gpio_put(dir_pins[i], 0);
        }
    }

    // EN pins — output, start LOW (drivers disabled at boot)
    
    const uint8_t en_pins[] = {
    EN_COIL_THUMB_LEFT_PIN,
    EN_COIL_COIL_LEFT_PIN
    };

    for (uint8_t i = 0; i < 2; i++) {
        if (en_pins[i] != 0xFF) {
            gpio_init(en_pins[i]);
            gpio_set_dir(en_pins[i], GPIO_OUT);
            gpio_put(en_pins[i], 0);
        }
    }

    // Button pins — input, pull up
    const uint8_t sw_pins[] = {
        SW1_PIN, SW2_PIN, SW3_PIN, SW4_PIN, SW5_PIN
    };

    for (uint8_t i = 0; i < 5; i++) {
        if (sw_pins[i] != 0xFF) {
            gpio_init(sw_pins[i]);
            gpio_set_dir(sw_pins[i], GPIO_IN);
            gpio_pull_up(sw_pins[i]);
        }
    }

}

// -----------------------------------------------------------------------------
// check_reset_reason()
// Log why the chip booted — critical for debugging in the field
// Watchdog resets must be visible immediately
// -----------------------------------------------------------------------------
static void check_reset_reason(void) {
    if (watchdog_caused_reboot()) {
        log_error("RESET CAUSE: Watchdog timeout — main loop stalled");
    } else {
        log_info("RESET CAUSE: Normal power-on or manual reset");
    }
}
