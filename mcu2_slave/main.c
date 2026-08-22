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
#include "pio_bus/pio_slave.h"

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
// LED heartbeat interval — matches MCU1 so both boards blink in step
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

    // Step 5 — Start the inter-MCU PIO bus (slave listens)
    if (pio_slave_init() != PIO_SLAVE_OK) {
        log_warning("PIO slave init failed — bus unavailable");
    }

    // Step 6 — Start watchdog
    watchdog_init();
    log_info("Watchdog started — 500ms timeout");

    // Boot complete
    log_info("MCU2 slave ready — awaiting PIO bus sync from MCU1");

    // -------------------------------------------------------------------------
    // Main loop
    // MCU2 is slave — it waits for commands from MCU1 via PIO bus
    // PIO bus driver added in next step
    // -------------------------------------------------------------------------
    uint32_t last_blink_ms = to_ms_since_boot(get_absolute_time());
    bool     led_on         = false;

    uint16_t bus_rx_count = 0;

    while (1) {

        // Feed watchdog — must happen every loop iteration
        watchdog_update();

        // PIO bus test — reply to whatever the master sends
        if (pio_slave_is_ready()) {
            proto_m2s_t in;
            if (pio_slave_receive(&in, 1000) == PIO_SLAVE_OK) {
                log_value("BUS RX, target0", in.coil_target[0]);
                proto_s2m_t out = {0};
                out.stick_x = (int16_t)(0x1000 + bus_rx_count++);
                pio_slave_send(&out);
            }
        }

        // LED heartbeat — non-blocking, proves the loop is running
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if ((now_ms - last_blink_ms) >= HEARTBEAT_MS) {
            last_blink_ms = now_ms;
            led_on = !led_on;
            if (MCU2_SPARE_PIN != 0xFF) {
                gpio_put(MCU2_SPARE_PIN, led_on);
            }
        }

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

    // Motor PWM + DIR — output, start LOW so no coil is driven at boot
    const uint8_t pwm_dir_pins[] = {
        S_PWM_COIL1_PIN, S_DIR_COIL1_PIN,
        S_PWM_COIL2_PIN, S_DIR_COIL2_PIN,
        S_PWM_COIL3_PIN, S_DIR_COIL3_PIN,
        S_PWM_COIL4_PIN, S_DIR_COIL4_PIN,
        S_PWM_VC1_PIN,   S_DIR_VC1_PIN
    };
    for (uint8_t i = 0; i < 10; i++) {
        gpio_init(pwm_dir_pins[i]);
        gpio_set_dir(pwm_dir_pins[i], GPIO_OUT);
        gpio_put(pwm_dir_pins[i], 0);
    }

    // DRV8873 chip selects — output, start HIGH (deselected)
    const uint8_t cs_pins[] = {
        S_SPI_CS_COIL1_PIN, S_SPI_CS_COIL2_PIN,
        S_SPI_CS_COIL3_PIN, S_SPI_CS_COIL4_PIN,
        S_SPI_CS_VC1_PIN
    };
    for (uint8_t i = 0; i < 5; i++) {
        gpio_init(cs_pins[i]);
        gpio_set_dir(cs_pins[i], GPIO_OUT);
        gpio_put(cs_pins[i], 1);
    }

    // Sensor chip selects — output, start HIGH (deselected)
    const uint8_t sensor_cs_pins[] = { S_SPI0_CS_PIN, S_SPI1_CS_PIN };
    for (uint8_t i = 0; i < 2; i++) {
        gpio_init(sensor_cs_pins[i]);
        gpio_set_dir(sensor_cs_pins[i], GPIO_OUT);
        gpio_put(sensor_cs_pins[i], 1);
    }

    // Encoder inputs — pull up
    const uint8_t wheel_pins[] = { WHEEL_1_PIN, WHEEL_2_PIN };
    for (uint8_t i = 0; i < 2; i++) {
        gpio_init(wheel_pins[i]);
        gpio_set_dir(wheel_pins[i], GPIO_IN);
        gpio_pull_up(wheel_pins[i]);
    }

    // MCU2 has NO plain LED. RGB_L / RGB_R are addressable strips needing a
    // bit-banged driver. FAULT lines are on the PCAL6416A, read over I2C.
    // The heartbeat below writes to a pin with no LED — harmless, no-op.
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
