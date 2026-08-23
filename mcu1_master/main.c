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
#include "usb_hid/hid.h"
#include "pio_bus/pio_master.h"

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
// LED heartbeat interval — proves the main loop is alive on real hardware
// -----------------------------------------------------------------------------
#define HEARTBEAT_MS        500

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

    // Step 5 — Start the inter-MCU PIO bus (master owns the clock)
    if (pio_master_init() != PIO_BUS_OK) {
        log_warning("PIO master init failed — bus unavailable");
    }

    // Step 6 — Start USB HID stack (before watchdog so tusb_init is never interrupted)
    hid_init();

    // Step 6 — Start watchdog (must feed it in main loop from this point on)
    watchdog_init();
    log_info("Watchdog started — 500ms timeout");

    // Boot complete
    log_info("MCU1 boot complete — entering main loop");

    // -------------------------------------------------------------------------
    // Main loop
    // Must call watchdog_update() every iteration — never block here
    // All work done via state machines and flags — no blocking calls
    // -------------------------------------------------------------------------
    uint32_t last_blink_ms = to_ms_since_boot(get_absolute_time());
    uint32_t bus_test_ms    = to_ms_since_boot(get_absolute_time());
    uint16_t bus_ping_count = 0;
    bool     bus_alive      = false;   // drives the LED rate
    bool     bus_ever_alive = false;   // latched — never goes back down
    bool     led_on         = false;

    while (1) {

        // Feed watchdog — must happen every loop iteration
        // If this stops being called — chip resets in 500ms
        watchdog_update();

        // Drive the USB stack — must run every iteration, never blocks
        hid_task();

        // PIO bus test — ping the slave every 100ms.
        // Result drives the LED: fast strobe = link up, slow = link down.
        if (pio_master_is_ready() &&
            (to_ms_since_boot(get_absolute_time()) - bus_test_ms) >= 100) {
            bus_test_ms = to_ms_since_boot(get_absolute_time());

            proto_m2s_t out = {0};
            out.coil_target[0] = (int16_t)bus_ping_count++;

            // Try a few times — one dropped packet should not read as dead.
            for (uint8_t attempt = 0; attempt < 5 && !bus_alive; attempt++) {
                if (pio_master_send(&out) != PIO_BUS_OK) {
                    continue;
                }
                proto_s2m_t in;
                pio_bus_result_t r = pio_master_receive(&in, 20000);

                // "Communicating" means bytes came back at all. A CRC or
                // packet-type mismatch still proves the wire is working,
                // which is the question this test is asking.
                if (r != PIO_BUS_ERR_TIMEOUT) {
                    bus_alive = true;
                    bus_ever_alive = true;
                    log_value("BUS bytes received, result", (int32_t)r);
                }
            }
            if (!bus_alive) {
                log_info("BUS silent — no bytes from slave");
            }
        }

        // LED heartbeat — non-blocking, proves the loop is running
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        // 8Hz strobe when the inter-MCU link is up, 1Hz when it is not
        uint32_t blink_ms = bus_ever_alive ? 60 : HEARTBEAT_MS;
        if ((now_ms - last_blink_ms) >= blink_ms) {
            last_blink_ms = now_ms;
            led_on = !led_on;
            if (M_LED_G_PIN != 0xFF) {
                gpio_put(M_LED_G_PIN, led_on);
            }
        }

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

    // Motor PWM + DIR — output, start LOW so no coil is driven at boot
    const uint8_t pwm_dir_pins[] = {
        M_PWM_COIL1_PIN, M_DIR_COIL1_PIN,
        M_PWM_COIL2_PIN, M_DIR_COIL2_PIN,
        M_PWM_COIL3_PIN, M_DIR_COIL3_PIN,
        M_PWM_COIL4_PIN, M_DIR_COIL4_PIN,
        M_PWM_VC1_PIN,   M_DIR_VC1_PIN
    };
    for (uint8_t i = 0; i < 10; i++) {
        gpio_init(pwm_dir_pins[i]);
        gpio_set_dir(pwm_dir_pins[i], GPIO_OUT);
        gpio_put(pwm_dir_pins[i], 0);
    }

    // DRV8873 chip selects — output, start HIGH (deselected)
    const uint8_t cs_pins[] = {
        M_SPI_CS_COIL1_PIN, M_SPI_CS_COIL2_PIN,
        M_SPI_CS_COIL3_PIN, M_SPI_CS_COIL4_PIN,
        M_SPI_CS_VC1_PIN
    };
    for (uint8_t i = 0; i < 5; i++) {
        gpio_init(cs_pins[i]);
        gpio_set_dir(cs_pins[i], GPIO_OUT);
        gpio_put(cs_pins[i], 1);
    }

    // Sensor chip selects — output, start HIGH (deselected)
    const uint8_t sensor_cs_pins[] = {
        M_SPI0_CS_PIN, M_SPI1_CS_PIN, M_SPI1_CS_AS_PIN, SPI_DISPLAY_CS_PIN
    };
    for (uint8_t i = 0; i < 4; i++) {
        gpio_init(sensor_cs_pins[i]);
        gpio_set_dir(sensor_cs_pins[i], GPIO_OUT);
        gpio_put(sensor_cs_pins[i], 1);
    }

    // SW1 — input, pull up. SW2-SW5 live on the PCAL6416A expander.
    // NOTE: SW1 is borrowed for UART1 TX during development.
    if (UART0_TX_PIN != SW1_PIN) {
        gpio_init(SW1_PIN);
        gpio_set_dir(SW1_PIN, GPIO_IN);
        gpio_pull_up(SW1_PIN);
    }

    // PCAL6416A interrupt — input, pull up (active low)
    gpio_init(INT_EXPANDER_PIN);
    gpio_set_dir(INT_EXPANDER_PIN, GPIO_IN);
    gpio_pull_up(INT_EXPANDER_PIN);

    // Status LED — the only plain LED on the board
    gpio_init(M_LED_G_PIN);
    gpio_set_dir(M_LED_G_PIN, GPIO_OUT);
    gpio_put(M_LED_G_PIN, 0);
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
