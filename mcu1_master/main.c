#include <string.h>
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
#include "sensors/tmag5170.h"
#include "sensors/as5047p.h"
#include "sensors/mcp9808.h"
#include "sensors/lsm6dsl.h"
#include "motor/drv8873.h"
#include "motor/coil_pwm.h"
#include "motor/current_sense.h"
#include "motor/current_limit.h"
#include "motor/coil_settings.h"
#include "motor/coil_selftest.h"
#include "motor/right_selftest.h"
#include "sensors/coil_interf.h"

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
// TMAG link status, shown on the green LED so the board can be checked
// without a UART adapter:
//   fast strobe  link OK
//   2 blinks     CRC mismatch
//   3 blinks     no reply, nothing driving MISO
//   4 blinks     PIO SPI init failed
// -----------------------------------------------------------------------------
static tmag_result_t tmag_status = TMAG_ERR_SPI;
static as_result_t   as_status   = AS_ERR_SPI;

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

    // Step 5 — Bring up the TMAG5170 hall sensor on SPI0
    tmag_status = tmag_init();
    as_status = as5047_init();
    mcp9808_init();
    lsm6dsl_init();
    drv_init_all();
    coil_pwm_init();
    current_sense_init();
    bool settings_loaded = coil_settings_load();   // saved amp settings

    // Step 6 — Start the inter-MCU PIO bus (master owns the clock)
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
    uint32_t tmag_ms        = to_ms_since_boot(get_absolute_time());
    hid_primary_report_t hid_report = {0};
    uint32_t last_force_ms = 0;      // 0 means no command yet
    uint16_t dbg_rx_count = 0;       // force commands received
    uint16_t dbg_max_level = 0;      // highest PWM level written
    uint8_t  dbg_drive_mask = 0;     // bit per coil driven by last command
    uint16_t dbg_fault_diag = 0;     // FAULT | DIAG<<8 of first driven coil
    uint16_t cfg_rx_count = 0;      // output reports received, any kind
    uint16_t cfg_cmd_seen = 0;      // first byte of the last one
    int16_t  vc_force_dbg = 0;      // force value parsed for VC1
    uint16_t post_drive_diag = 0;   // FAULT|DIAG read right after driving
    bool     bus_alive      = false;   // drives the LED rate
    bool     bus_ever_alive = false;   // latched — never goes back down
    bool     led_on         = false;
    bool     limits_reply_pending = false;
    bool     selftest_reply_pending = false;

    // Right side, reached through MCU2 over the PIO link
    int16_t  right_force[DRV_COUNT] = {0};   // forces for the right coils
    uint32_t right_force_ms = 0;             // last 0x14 from the host, 0 = none
    uint16_t right_ma[DRV_COUNT] = {0};      // right coil currents (mA), from MCU2
    uint8_t  right_status = 0;               // MCU2 guard bits (half power), bit per coil
    uint16_t right_rx_ok = 0, right_rx_err = 0;    // MCU2's own packet counters
    uint16_t link_ok = 0, link_timeout = 0, link_crc = 0, link_bad = 0;
    uint32_t link_last_ok_ms = 0;
    uint32_t link_ms = 0;
    bool     right_reply_pending = false;
    bool     dump_reply_pending  = false;
    bool     sensors_reply_pending = false;
    bool     interf_reply_pending  = false;
    bool     rst_reply_pending     = false;
    bool     rst_ever_run          = false;
    uint8_t  rst_part              = 0;

    // Latest sensor readings, refreshed at 50 Hz, for the Sensors card
    tmag_xyz_t    sens_xyz = {0, 0, 0};
    tmag_result_t sens_tmag_r = TMAG_ERR_SPI;
    uint16_t      sens_ang = 0;
    as_result_t   sens_as_r = AS_ERR_SPI;
    as_diag_t     sens_diag = {0};
    bool     settings_saved = settings_loaded;   // flash matches live values

    while (1) {

        // Feed watchdog — must happen every loop iteration
        // If this stops being called — chip resets in 500ms
        watchdog_update();

        // Drive the USB stack — must run every iteration, never blocks
        hid_task();

        // Force commands from the host. Report ID 2, byte 0 is the command,
        // 0x10 followed by five signed 16-bit values, little-endian, for
        // coils 1-4 then the voice coil.
        #define CMD_SET_FORCES 0x10
        // 0x11 followed by five unsigned 16-bit limits in mA, little-endian,
        // coils 1-4 then the voice coil. 0 = no limit.
        #define CMD_SET_LIMITS 0x11
        // 0x12 asks for the current amp settings. The reply (and the reply
        // to 0x11) goes back as config report ID 2:
        //   [0x12][5 x uint16 mA][saved: 1 = flash holds these values]
        #define CMD_GET_LIMITS 0x12
        // 0x13 runs the coil self-test. Reply, config report ID 2:
        //   [0x13][per coil x5: idle_raw u16, mA@50% u16, mA@100% u16,
        //          mA@-100% u16, FAULT u8, DIAG u8, IC1 u8, reconfig u8]
        //   12 bytes x 5 = 60, fits the 62-byte payload.
        #define CMD_SELF_TEST  0x13
        // 0x14: five int16 forces for the RIGHT coils (same layout as 0x10),
        //       relayed to MCU2 over the PIO link.
        // 0x15: request right-side status. Reply, config report ID 2:
        //   [0x15][5 x mA u16][guard bits u8][MCU2 rx ok u16][MCU2 rx err u16]
        //   [link ok u16][timeouts u16][crc errs u16][bad pkts u16][fresh u8]
        // 0x16: request a raw register dump of the five left DRV8873s.
        //   Reply: [0x16][driver x5: regs 0x00..0x05 as raw 16-bit SPI replies]
        #define CMD_SET_FORCES_R 0x14
        #define CMD_GET_RIGHT    0x15
        #define CMD_DRV_DUMP     0x16
        // 0x17: request latest sensor readings. Reply:
        //   [0x17][TMAG init u8][TMAG read u8][X i16][Y i16][Z i16][wiring u8]
        //   [AS init u8][AS read u8][angle u16][AGC u8][flags u8: MAGH,MAGL,COF,LF]
        // 0x18: run the coil interference test. Reply when done:
        //   [0x18][base X,Y,Z i16][base angle u16][base ok u8]
        //   [per coil x5: dX,dY,dZ,dAngle i16, ok u8]
        #define CMD_GET_SENSORS  0x17
        #define CMD_INTERF_TEST  0x18
        // 0x19: payload[0] = 1 starts the RIGHT coil self-test (about 11 s).
        //   Any 0x19 also asks for the result. Reply, config report ID 2:
        //   [0x19][state: 0 never run, 1 running, 2 done][progress u8]
        //   [link ok u16][timeouts u16][crc u16][bad u16]
        //   [MCU2 fw u8][MCU2 flags u8][MCU2 reset u8][MCU2 rx err u8]
        //   [part u8: 0 or 1]
        //   part 0: [link ok u16][timeouts u16][crc u16][bad u16]
        //           [MCU2 fw u8][MCU2 flags u8][MCU2 reset u8][MCU2 rx err u8]
        //           coils 1-2
        //   part 1: coils 3-5
        //   each coil, 19 bytes, mA as u16: [rest][+50%][+100% avg]
        //   [+100% peak][-100% avg][-100% peak][pins u16][guard u8]
        //   [other coil u8][other mA u16][samples u8]
        //   While running only part 0 is sent (progress). When done, both.
        #define CMD_RIGHT_TEST   0x19
        if (hid_get_config_received()) {
            hid_config_report_t cfg;
            hid_get_last_config(&cfg);
            cfg_rx_count++;
            cfg_cmd_seen = cfg.command;
            if (cfg.command == CMD_SET_FORCES && !selftest_running() && !interf_running()) {
                for (uint8_t i = 0; i < DRV_COUNT; i++) {
                    int16_t f = (int16_t)((uint16_t)cfg.payload[i*2] |
                                          ((uint16_t)cfg.payload[i*2+1] << 8));
                    coil_set_force((drv_id_t)i, f);
                    if (i == DRV_VC1) vc_force_dbg = f;
                }

                // Open load is only detectable once the bridge has actually
                // driven, so re-read the fault registers here rather than
                // while idle. Datasheet 7.3.2: OLD asserts on open load.
                // Give the driver a moment to evaluate before reading.
                busy_wait_us(500);
                uint8_t pf = 0, pd = 0;
                drv_read_reg(DRV_VC1, DRV_REG_FAULT, &pf, NULL);
                drv_read_reg(DRV_VC1, DRV_REG_DIAG,  &pd, NULL);
                post_drive_diag = (uint16_t)(pf | ((uint16_t)pd << 8));
                last_force_ms = to_ms_since_boot(get_absolute_time());
                dbg_rx_count++;
                // What this command actually drove: which coils, the PWM
                // level, and the FAULT/DIAG of the first driven coil.
                dbg_drive_mask = 0;
                dbg_max_level  = 0;
                dbg_fault_diag = 0;
                for (uint8_t i = 0; i < DRV_COUNT; i++) {
                    uint16_t lv = coil_get_level((drv_id_t)i);
                    if (lv == 0) continue;
                    if (dbg_drive_mask == 0) {
                        busy_wait_us(500);
                        uint8_t fr = 0, dg = 0;
                        drv_read_reg((drv_id_t)i, DRV_REG_FAULT, &fr, NULL);
                        drv_read_reg((drv_id_t)i, DRV_REG_DIAG,  &dg, NULL);
                        dbg_fault_diag = (uint16_t)(fr | ((uint16_t)dg << 8));
                    }
                    dbg_drive_mask |= (uint8_t)(1u << i);
                    if (lv > dbg_max_level) dbg_max_level = lv;
                }
            } else if (cfg.command == CMD_SET_LIMITS) {
                for (uint8_t i = 0; i < DRV_COUNT; i++) {
                    uint16_t ma = (uint16_t)((uint16_t)cfg.payload[i*2] |
                                             ((uint16_t)cfg.payload[i*2+1] << 8));
                    if (ma > 2420u) ma = 2420u;     // sense full scale
                    current_limit_set_ma((drv_id_t)i, ma);
                }
                settings_saved = coil_settings_save();
                limits_reply_pending = true;
            } else if (cfg.command == CMD_GET_LIMITS) {
                limits_reply_pending = true;
            } else if (cfg.command == CMD_SELF_TEST) {
                if (!selftest_running() && !interf_running()) {
                    last_force_ms = 0;          // keep the timeout out of it
                    selftest_start();
                }
            } else if (cfg.command == CMD_SET_FORCES_R && !rst_running()) {
                for (uint8_t i = 0; i < DRV_COUNT; i++) {
                    right_force[i] = (int16_t)((uint16_t)cfg.payload[i*2] |
                                               ((uint16_t)cfg.payload[i*2+1] << 8));
                }
                right_force_ms = to_ms_since_boot(get_absolute_time());
            } else if (cfg.command == CMD_GET_RIGHT) {
                right_reply_pending = true;
            } else if (cfg.command == CMD_DRV_DUMP) {
                dump_reply_pending = true;
            } else if (cfg.command == CMD_GET_SENSORS) {
                sensors_reply_pending = true;
            } else if (cfg.command == CMD_RIGHT_TEST) {
                if (cfg.payload[0] == 1 && !rst_running()) {
                    rst_start(to_ms_since_boot(get_absolute_time()));
                    rst_ever_run = true;
                }
                rst_reply_pending = true;
                rst_part = 0;
            } else if (cfg.command == CMD_INTERF_TEST) {
                if (!selftest_running() && !interf_running()) {
                    last_force_ms = 0;
                    interf_start();
                }
            }
        }

        // Interference test runs alongside everything else; reply when done.
        if (interf_task()) interf_reply_pending = true;

        // Sensors reply
        if (sensors_reply_pending) {
            hid_config_report_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.command = CMD_GET_SENSORS;
            uint8_t *q = rep.payload;
            extern uint8_t tmag_wiring;
            *q++ = (uint8_t)tmag_status;
            *q++ = (uint8_t)sens_tmag_r;
            const int16_t v3[3] = { sens_xyz.x, sens_xyz.y, sens_xyz.z };
            for (uint8_t i = 0; i < 3; i++) {
                *q++ = (uint8_t)((uint16_t)v3[i] & 0xFF); *q++ = (uint8_t)((uint16_t)v3[i] >> 8);
            }
            *q++ = tmag_wiring;
            *q++ = (uint8_t)as_status;
            *q++ = (uint8_t)sens_as_r;
            *q++ = (uint8_t)(sens_ang & 0xFF); *q++ = (uint8_t)(sens_ang >> 8);
            *q++ = sens_diag.agc;
            *q++ = (uint8_t)((sens_diag.magh ? 1u : 0u) | (sens_diag.magl ? 2u : 0u) |
                             (sens_diag.cof  ? 4u : 0u) | (sens_diag.lf   ? 8u : 0u));
            if (hid_send_config(&rep)) sensors_reply_pending = false;
        }

        // Interference test reply
        if (interf_reply_pending && !sensors_reply_pending) {
            hid_config_report_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.command = CMD_INTERF_TEST;
            uint8_t *q = rep.payload;
            const interf_base_t *bs = interf_baseline();
            const int16_t b3[3] = { bs->x, bs->y, bs->z };
            for (uint8_t i = 0; i < 3; i++) {
                *q++ = (uint8_t)((uint16_t)b3[i] & 0xFF); *q++ = (uint8_t)((uint16_t)b3[i] >> 8);
            }
            *q++ = (uint8_t)(bs->ang & 0xFF); *q++ = (uint8_t)(bs->ang >> 8);
            *q++ = bs->ok;
            const interf_result_t *ir = interf_results();
            for (uint8_t c = 0; c < DRV_COUNT; c++) {
                const int16_t d4[4] = { ir[c].dx, ir[c].dy, ir[c].dz, ir[c].dang };
                for (uint8_t i = 0; i < 4; i++) {
                    *q++ = (uint8_t)((uint16_t)d4[i] & 0xFF); *q++ = (uint8_t)((uint16_t)d4[i] >> 8);
                }
                *q++ = ir[c].ok;
            }
            if (hid_send_config(&rep)) interf_reply_pending = false;
        }

        // Right coil self-test: advance it, reply when done or when asked
        if (rst_task(to_ms_since_boot(get_absolute_time()))) {
            rst_reply_pending = true;
            rst_part = 0;
        }
        if (rst_reply_pending && !sensors_reply_pending && !interf_reply_pending) {
            hid_config_report_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.command = CMD_RIGHT_TEST;
            uint8_t *q = rep.payload;
            const rst_result_t *rr = rst_results();
            bool run = rst_running();
            *q++ = run ? 1u : (rst_ever_run ? 2u : 0u);
            *q++ = rst_progress();
            *q++ = rst_part;
            // Bounds-checked writes (part 0 uses 53 bytes, part 1 uses 60, of 62)
            uint8_t * const q_end = rep.payload + sizeof(rep.payload);
            #define PUT16(v) do { uint16_t _v = (uint16_t)(v); \
                if (q + 2 <= q_end) { *q++ = (uint8_t)(_v & 0xFF); *q++ = (uint8_t)(_v >> 8); } } while (0)
            #define PUT8(v) do { if (q < q_end) *q++ = (uint8_t)(v); } while (0)
            uint8_t c0 = 0, c1 = 2;
            if (rst_part == 0) {
                PUT16(rr->link_ok); PUT16(rr->link_timeout);
                PUT16(rr->link_crc); PUT16(rr->link_bad);
                PUT8(rr->slave_fw); PUT8(rr->slave_flags);
                PUT8(rr->slave_reset); PUT8(rr->slave_rx_err);
            } else {
                c0 = 2; c1 = 5;
            }
            for (uint8_t c = c0; c < c1; c++) {
                const rst_coil_t *k = &rr->coil[c];
                PUT16(k->idle_ma); PUT16(k->f50_ma); PUT16(k->f100_ma);
                PUT16(k->f100_peak); PUT16(k->rev_ma); PUT16(k->rev_peak);
                PUT16(k->pins);
                PUT8(k->guard); PUT8(k->other_idx);
                PUT16(k->other_ma);
                PUT8(k->samples);
            }
            #undef PUT16
            #undef PUT8
            if (hid_send_config(&rep)) {
                // Progress polls get part 0 only; a finished test sends both
                if (rst_part == 0 && !run && rst_ever_run) rst_part = 1;
                else { rst_reply_pending = false; rst_part = 0; }
            }
        }

        // Right-side status reply
        if (right_reply_pending) {
            hid_config_report_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.command = CMD_GET_RIGHT;
            uint8_t *q = rep.payload;
            for (uint8_t i = 0; i < DRV_COUNT; i++) {
                *q++ = (uint8_t)(right_ma[i] & 0xFF); *q++ = (uint8_t)(right_ma[i] >> 8);
            }
            *q++ = right_status;
            const uint16_t w[6] = { right_rx_ok, right_rx_err, link_ok,
                                    link_timeout, link_crc, link_bad };
            for (uint8_t i = 0; i < 6; i++) {
                *q++ = (uint8_t)(w[i] & 0xFF); *q++ = (uint8_t)(w[i] >> 8);
            }
            uint32_t nowr = to_ms_since_boot(get_absolute_time());
            *q++ = (link_last_ok_ms != 0 && (nowr - link_last_ok_ms) < 500u) ? 1u : 0u;
            if (hid_send_config(&rep)) right_reply_pending = false;
        }

        // Raw DRV8873 register dump (left side), to see whether SPI reads
        // and writes work. Datasheet defaults: IC1 0x51, IC2 0x0C, IC3 0x40,
        // IC4 0x08 (0x18 once our open-load write has landed).
        if (dump_reply_pending && !right_reply_pending) {
            extern uint16_t drv_last_rx[DRV_COUNT];
            hid_config_report_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.command = CMD_DRV_DUMP;
            uint8_t *q = rep.payload;
            for (uint8_t d = 0; d < DRV_COUNT; d++) {
                for (uint8_t reg = 0; reg <= 5; reg++) {
                    drv_last_rx[d] = 0;
                    drv_read_reg((drv_id_t)d, reg, NULL, NULL);
                    uint16_t raw = drv_last_rx[d];
                    *q++ = (uint8_t)(raw & 0xFF); *q++ = (uint8_t)(raw >> 8);
                }
            }
            if (hid_send_config(&rep)) dump_reply_pending = false;
        }

        // Coil self-test runs alongside everything else; reply when done.
        if (selftest_task()) selftest_reply_pending = true;
        if (selftest_reply_pending && !limits_reply_pending) {
            hid_config_report_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.command = CMD_SELF_TEST;
            const selftest_result_t *r = selftest_results();
            for (uint8_t i = 0; i < DRV_COUNT; i++) {
                uint8_t *p = &rep.payload[i * 12];
                p[0] = (uint8_t)(r[i].idle_raw & 0xFF);
                p[1] = (uint8_t)(r[i].idle_raw >> 8);
                p[2] = (uint8_t)(r[i].ma & 0xFF);
                p[3] = (uint8_t)(r[i].ma >> 8);
                p[4] = (uint8_t)(r[i].ma_full & 0xFF);
                p[5] = (uint8_t)(r[i].ma_full >> 8);
                p[6] = (uint8_t)(r[i].ma_rev & 0xFF);
                p[7] = (uint8_t)(r[i].ma_rev >> 8);
                p[8] = r[i].fault;
                p[9] = r[i].diag;
                p[10] = r[i].ic1;
                p[11] = r[i].reconfig;
            }
            if (hid_send_config(&rep)) selftest_reply_pending = false;
        }

        // Answer a settings request as soon as the endpoint is free.
        if (limits_reply_pending) {
            hid_config_report_t rep;
            memset(&rep, 0, sizeof(rep));
            rep.command = CMD_GET_LIMITS;
            for (uint8_t i = 0; i < DRV_COUNT; i++) {
                uint16_t ma = current_limit_get_ma((drv_id_t)i);
                rep.payload[i*2]     = (uint8_t)(ma & 0xFF);
                rep.payload[i*2 + 1] = (uint8_t)(ma >> 8);
            }
            rep.payload[DRV_COUNT * 2] = settings_saved ? 1u : 0u;
            if (hid_send_config(&rep)) limits_reply_pending = false;
        }

        // Hold each coil at or under its current limit.
        current_limit_task();

        // Every 200 ms make sure each driver is still in PH/EN with open-load
        // detection on, and put it back if not (counted, shown in self-test).
        {
            static uint32_t cfg_check_ms = 0;
            uint32_t nowc = to_ms_since_boot(get_absolute_time());
            if (nowc - cfg_check_ms >= 200u) {
                cfg_check_ms = nowc;
                for (uint8_t i = 0; i < DRV_COUNT; i++) {
                    drv_ensure_config((drv_id_t)i, NULL);
                }
            }
        }

        // Safety: stop the coils if the host goes quiet. A closed browser tab
        // should not leave them energised.
        if (last_force_ms != 0 &&
            (to_ms_since_boot(get_absolute_time()) - last_force_ms) > 500) {
            coil_all_off();
            last_force_ms = 0;
            dbg_drive_mask = 0;
            dbg_max_level  = 0;
        }

        // Right side: exchange one packet with MCU2 every 10 ms.
        // Out: the five right-coil forces. Back: right currents, guard bits
        // and MCU2's packet counters. Every result is counted so the
        // dashboard shows whether the link really delivers intact packets.
        // Every 10 ms while the right board answers; every 100 ms while it
        // is silent (unplugged), so a missing board costs little loop time.
        uint32_t link_period = (link_last_ok_ms != 0 &&
            (to_ms_since_boot(get_absolute_time()) - link_last_ok_ms) < 500u) ? 10u : 100u;
        if (pio_master_is_ready() &&
            (to_ms_since_boot(get_absolute_time()) - link_ms) >= link_period) {
            link_ms = to_ms_since_boot(get_absolute_time());

            // Host went quiet on the right side -> command zero
            if (right_force_ms != 0 && (link_ms - right_force_ms) > 500u) {
                for (uint8_t i = 0; i < DRV_COUNT; i++) right_force[i] = 0;
                right_force_ms = 0;
            }

            proto_m2s_t out = {0};
            for (uint8_t i = 0; i < DRV_COUNT; i++) out.coil_target[i] = right_force[i];
            if (rst_running()) {
                // Self-test owns the right coils; the host's forces wait
                uint8_t probe = 0;
                int16_t tf[DRV_COUNT];     // local copy: out is a packed struct
                rst_fill(link_ms, tf, &probe);
                for (uint8_t i = 0; i < DRV_COUNT; i++) out.coil_target[i] = tf[i];
                out.reserved[0] = probe;
            }

            if (pio_master_send(&out) == PIO_BUS_OK) {
                proto_s2m_t in;
                pio_bus_result_t r = pio_master_receive(&in, 3000);
                {
                    rst_reply_t rp;
                    memset(&rp, 0, sizeof(rp));
                    int rr = RST_LINK_BAD;
                    if (r == PIO_BUS_OK) {
                        rr = RST_LINK_OK;
                        for (uint8_t i = 0; i < DRV_COUNT; i++) rp.ma[i] = in.coil_current[i];
                        rp.status = in.status;
                        rp.sx = (uint16_t)in.stick_x;
                        rp.sy = (uint16_t)in.stick_y;
                        rp.sz = (uint16_t)in.stick_z;
                        rp.rx_err = (uint16_t)(in.reserved[2] | (in.reserved[3] << 8));
                    } else if (r == PIO_BUS_ERR_TIMEOUT) {
                        rr = RST_LINK_TIMEOUT;
                    } else if (r == PIO_BUS_ERR_CRC) {
                        rr = RST_LINK_CRC;
                    }
                    rst_on_reply(to_ms_since_boot(get_absolute_time()), rr, &rp);
                }
                if (r == PIO_BUS_OK) {
                    link_ok++;
                    link_last_ok_ms = to_ms_since_boot(get_absolute_time());
                    for (uint8_t i = 0; i < DRV_COUNT; i++) right_ma[i] = in.coil_current[i];
                    right_status = in.status;
                    right_rx_ok  = (uint16_t)(in.reserved[0] | (in.reserved[1] << 8));
                    right_rx_err = (uint16_t)(in.reserved[2] | (in.reserved[3] << 8));
                    bus_alive = true;
                    bus_ever_alive = true;
                } else if (r == PIO_BUS_ERR_TIMEOUT) {
                    link_timeout++;
                } else if (r == PIO_BUS_ERR_CRC) {
                    link_crc++;
                } else {
                    link_bad++;
                }
            }
            if (link_last_ok_ms == 0 ||
                (to_ms_since_boot(get_absolute_time()) - link_last_ok_ms) > 500u) {
                bus_alive = false;
            }
            (void)bus_alive; (void)bus_ever_alive;   // kept for future status use
        }

        // Sensor read at 50Hz, logged so the values can be checked
        if ((to_ms_since_boot(get_absolute_time()) - tmag_ms) >= 20) {
            tmag_ms = to_ms_since_boot(get_absolute_time());
            tmag_xyz_t m;
            sens_tmag_r = tmag_read_xyz(&m);
            if (sens_tmag_r == TMAG_OK) sens_xyz = m;
            if (sens_tmag_r == TMAG_OK) {
                log_value("TMAG X", m.x);
                log_value("TMAG Y", m.y);
                log_value("TMAG Z", m.z);
                hid_report.stick1_x = m.x;
                hid_report.stick1_y = m.y;
            }
            // AS5047 rotation angle feeds stick 1's second axis once the
            // sensor board is working; unused while it is not.
            uint16_t ang;
            sens_as_r = as5047_read_angle(&ang);
            if (sens_as_r == AS_OK) {
                sens_ang = ang;
                as_diag_t dg;
                if (as5047_read_diag(&dg) == AS_OK) sens_diag = dg;
            }
            if (sens_as_r == AS_OK) {
                log_value("AS5047 angle", ang);
            }

            // -----------------------------------------------------------------
            // Telemetry layout. ONE write per slot — an earlier version had
            // several values sharing slots, which silently overwrote the gyro.
            //   0-4  coil current in mA: coil 1-4 then voice coil
            //   5    temperature, hundredths of a degree C
            //   6-8  gyro X, Y, Z
            //   9    status: low byte = driver present mask,
            //               high byte = health flags
            // -----------------------------------------------------------------
            for (uint8_t i = 0; i < DRV_COUNT; i++) {
                hid_report.coil_current[i] = current_coil_ma((drv_id_t)i);
            }

            float temp_c = 0.0f;
            bool temp_ok = (mcp9808_read_temp(&temp_c) == MCP_OK);
            hid_report.coil_current[5] =
                temp_ok ? (uint16_t)(int16_t)(temp_c * 100.0f) : 0;

            lsm_xyz_t g = {0, 0, 0};
            bool gyro_ok = (lsm6dsl_read_gyro(&g) == LSM_OK);
            hid_report.coil_current[6] = (uint16_t)g.x;
            hid_report.coil_current[7] = (uint16_t)g.y;
            hid_report.coil_current[8] = (uint16_t)g.z;

            uint8_t health = 0;
            if (temp_ok)                 health |= 0x01;
            if (gyro_ok)                 health |= 0x02;
            if (tmag_status == TMAG_OK)  health |= 0x04;
            if (as_status   == AS_OK)    health |= 0x08;
            extern uint16_t mcp_found_20_27;
            if (mcp_found_20_27)         health |= 0x10;

            // TEMPORARY: stick 2 is unused until MCU2 relays its data, so
            // borrow it to show the force path. X = commands received,
            // Y = highest PWM level written (2499 = 50% cap).
            // TEMPORARY force debug until MCU2 relays stick 2 and the
            // button expander is fitted:
            //   stick2_x  FAULT | DIAG<<8 of the first driven coil
            //   stick2_y  PWM level being driven (2499 = 50%, 4999 = 100%)
            //   buttons   bit per coil driven, bit 0 = coil 1, bit 4 = VC1
            (void)dbg_rx_count;
            hid_report.stick2_x = (int16_t)dbg_fault_diag;
            // Live level after the current limit, not the requested one
            uint16_t live_lvl = 0;
            for (uint8_t i = 0; i < DRV_COUNT; i++) {
                uint16_t lv = coil_get_level((drv_id_t)i);
                if (lv > live_lvl) live_lvl = lv;
            }
            (void)dbg_max_level;
            hid_report.stick2_y = (int16_t)live_lvl;
            hid_report.buttons  = dbg_drive_mask;

            hid_report.coil_current[9] =
                (uint16_t)(drv_present_mask() | ((uint16_t)health << 8));

            hid_send_primary(&hid_report);
            {
            }
        }

        // LED heartbeat — non-blocking, proves the loop is running
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        // TMAG OK: fast strobe. Otherwise blink an error count, then pause.
        if (tmag_status == TMAG_OK && as_status == AS_OK) {
            if ((now_ms - last_blink_ms) >= 60) {
                last_blink_ms = now_ms;
                led_on = !led_on;
                gpio_put(M_LED_G_PIN, led_on);
            }
        } else {
            // 2 blinks TMAG bad, 3 AS5047 bad, 4 both bad
            uint8_t count;
            if (tmag_status != TMAG_OK && as_status != AS_OK)      count = 4;
            else if (tmag_status != TMAG_OK)                       count = 2;
            else                                                   count = 3;
            uint32_t period = (uint32_t)count * 400u + 1200u;   // blinks + pause
            uint32_t t = now_ms % period;
            bool on = (t < (uint32_t)count * 400u) && ((t % 400u) < 200u);
            gpio_put(M_LED_G_PIN, on);
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
