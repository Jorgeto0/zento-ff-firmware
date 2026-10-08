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
#include "motor/coil_pwm.h"
#include "motor/current_sense.h"

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

// Right stick coils above this current drop to half power (see main loop)
#define STICK_GUARD_MA      750u

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

    // Right-side coils: both inputs low = coast, no force at boot
    coil_pwm_init();
    current_sense_init();

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

    // Link counters, sent back to MCU1 in reserved[] so the dashboard can
    // show whether packets arrive intact.
    uint16_t rx_ok  = 0;
    uint16_t rx_err = 0;

    // Last measured coil currents (mA) and guard state, sent in each reply.
    uint16_t coil_ma[DRV_COUNT] = {0};
    uint8_t  guard_mask = 0;      // bit per stick coil held at half power
    uint32_t last_cmd_ms = 0;     // 0 = no valid command yet / timed out

    while (1) {

        // Feed watchdog — must happen every loop iteration
        watchdog_update();
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());

        // MCU1 sends one packet about every 10 ms. Wait up to 20 ms for it.
        if (pio_slave_is_ready()) {
            proto_m2s_t in;
            pio_slave_result_t r = pio_slave_receive(&in, 20000);

            if (r == PIO_SLAVE_OK) {
                rx_ok++;

                // 1. Apply the five forces. A coil commanded to 0 also has
                //    its overcurrent guard released.
                for (uint8_t i = 0; i < DRV_COUNT; i++) {
                    if (in.coil_target[i] == 0 && (guard_mask & (1u << i))) {
                        guard_mask &= (uint8_t)~(1u << i);
                        coil_set_scale_q16((drv_id_t)i, COIL_SCALE_ONE);
                    }
                    coil_set_force((drv_id_t)i, in.coil_target[i]);
                }
                last_cmd_ms = to_ms_since_boot(get_absolute_time());

                // 2. Reply straight away with what we measured last cycle.
                proto_s2m_t out = {0};
                for (uint8_t i = 0; i < DRV_COUNT; i++) {
                    out.coil_current[i] = coil_ma[i];
                }
                out.status = (uint8_t)(guard_mask & 0x1Fu);
                out.reserved[0] = (uint8_t)(rx_ok & 0xFF);
                out.reserved[1] = (uint8_t)(rx_ok >> 8);
                out.reserved[2] = (uint8_t)(rx_err & 0xFF);
                out.reserved[3] = (uint8_t)(rx_err >> 8);
                pio_slave_send(&out);

                // 3. Measure for the next reply, while the bus is quiet.
                for (uint8_t i = 0; i < DRV_COUNT; i++) {
                    coil_ma[i] = current_coil_ma((drv_id_t)i);
                }

                // Overcurrent guard for the stick coils. Their rail (6V_2)
                // is unmeasured and may be 12V, so above STICK_GUARD_MA a
                // coil is held at half power until it is commanded to 0.
                for (uint8_t i = 0; i < DRV_VC1; i++) {
                    if (coil_ma[i] > STICK_GUARD_MA && !(guard_mask & (1u << i))) {
                        guard_mask |= (uint8_t)(1u << i);
                        coil_set_scale_q16((drv_id_t)i, COIL_SCALE_ONE / 2u);
                    }
                }
            } else if (r != PIO_SLAVE_ERR_TIMEOUT) {
                rx_err++;
            }
        }

        // Safety: no valid command for 500 ms (cable out, MCU1 reset,
        // dashboard closed) -> all right-side coils off.
        now_ms = to_ms_since_boot(get_absolute_time());
        if (last_cmd_ms != 0 && (now_ms - last_cmd_ms) > 500u) {
            coil_all_off();
            last_cmd_ms = 0;
        }

        // Heartbeat on a spare pin (no LED on MCU2) — proves the loop runs
        if ((now_ms - last_blink_ms) >= HEARTBEAT_MS) {
            last_blink_ms = now_ms;
            led_on = !led_on;
            if (MCU2_SPARE_PIN != 0xFF) {
                gpio_put(MCU2_SPARE_PIN, led_on);
            }
        }
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
