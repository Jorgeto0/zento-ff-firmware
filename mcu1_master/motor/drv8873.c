// =============================================================================
// mcu1_master/motor/drv8873.c — DRV8873S driver
// Verified against TI datasheet SLVSET1
// =============================================================================

#include "drv8873.h"
#include "spi_pio_m1.h"
#include "config.h"
#include "diagnostics/uart_log.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#define DRV_SPI_HZ   2000000u   // datasheet allows 10 MHz; 2 MHz is safe on
                                // a shared bus reaching five devices

static spi_m1_t bus;
static bool     ready = false;
static uint8_t  present = 0;
uint16_t drv_last_rx[DRV_COUNT] = {0};   // raw reply per driver, for diagnosis

// Chip selects, one per driver, in drv_id_t order
static const uint8_t cs_pin[DRV_COUNT] = {
    M_SPI_CS_COIL1_PIN, M_SPI_CS_COIL2_PIN,
    M_SPI_CS_COIL3_PIN, M_SPI_CS_COIL4_PIN,
    M_SPI_CS_VC1_PIN
};

uint8_t drv_present_mask(void) { return present; }

// One 16-bit frame. CS low for the whole frame, then >=500 ns high (7.5.1).
static uint16_t frame(drv_id_t id, uint16_t tx) {
    gpio_put(cs_pin[id], 0);
    uint16_t rx = (uint16_t)spi_m1_xfer(&bus, tx, 16);
    gpio_put(cs_pin[id], 1);
    drv_last_rx[id] = rx;   // keep every reply so a failure can be diagnosed
    busy_wait_us(1);
    return rx;
}

drv_result_t drv_read_reg(drv_id_t id, uint8_t addr, uint8_t *out, uint8_t *status) {
    if (!ready) return DRV_ERR_SPI;

    // [15]=0 [14]=1 read [13:9]=addr [8]=X [7:0]=don't care
    uint16_t rx = frame(id, (uint16_t)((1u << 14) | ((addr & 0x1F) << 9)));

    // Bits 15 and 14 of the reply are always 1 on a live device (7.5.1.2),
    // so all-zeros or all-ones means nothing is answering.
    if (rx == 0x0000u || rx == 0xFFFFu) return DRV_ERR_DEAD;
    if ((rx & 0xC000u) != 0xC000u)      return DRV_ERR_DEAD;

    if (out)    *out    = (uint8_t)(rx & 0xFF);
    if (status) *status = (uint8_t)((rx >> 8) & 0x3F);
    return DRV_OK;
}

drv_result_t drv_write_reg(drv_id_t id, uint8_t addr, uint8_t value) {
    if (!ready) return DRV_ERR_SPI;
    frame(id, (uint16_t)(((addr & 0x1F) << 9) | value));   // [14]=0 write
    return DRV_OK;
}

drv_result_t drv_read_fault(drv_id_t id, uint8_t *fault, uint8_t *diag) {
    drv_result_t r = drv_read_reg(id, DRV_REG_FAULT, fault, NULL);
    if (r != DRV_OK) return r;
    return drv_read_reg(id, DRV_REG_DIAG, diag, NULL);
}

drv_result_t drv_init_all(void) {
    // Same bus type as the AS5047: mode 1, 16-bit. pio1 SM2 — SM0 is the
    // TMAG, SM1 the AS5047.
    if (!spi_m1_init(&bus, pio1, 2,
                     M_SPI_DI_COIL_PIN,     // MOSI, into each driver's SDI
                     M_SPI_DO_COIL_PIN,     // MISO, from each driver's SDO
                     M_SPI_SCK_COIL_PIN,
                     DRV_SPI_HZ)) {
        log_error("DRV8873 PIO SPI init failed");
        return DRV_ERR_SPI;
    }

    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        gpio_init(cs_pin[i]);
        gpio_set_dir(cs_pin[i], GPIO_OUT);
        gpio_put(cs_pin[i], 1);
    }

    ready = true;
    present = 0;

    for (uint8_t i = 0; i < DRV_COUNT; i++) {
        // One dummy frame to settle the shared bus, then a few attempts.
        // In testing only the last driver in this loop answered, which is the
        // signature of the bus needing a transaction or two before it is
        // reliable rather than four devices being absent.
        uint8_t ic1;
        drv_read_reg((drv_id_t)i, DRV_REG_FAULT, NULL, NULL);

        bool answered = false;
        for (uint8_t attempt = 0; attempt < 3 && !answered; attempt++) {
            if (drv_read_reg((drv_id_t)i, DRV_REG_IC1, &ic1, NULL) == DRV_OK) {
                answered = true;
            } else {
                busy_wait_us(100);
            }
        }
        if (!answered) {
            continue;               // not fitted, or not answering
        }
        present |= (uint8_t)(1u << i);

        // IC1 is lock-protected. Unlock, set PH/EN mode, leave unlocked so
        // slew rate can be tuned later. Table 4: MODE 00b is PH/EN, which is
        // how this board wires EN/IN1 and PH/IN2.
        drv_write_reg((drv_id_t)i, DRV_REG_IC3,
                      (uint8_t)(DRV_LOCK_UNLOCK << DRV_LOCK_SHIFT));

        uint8_t v = (uint8_t)((ic1 & ~0x03u) | DRV_MODE_PH_EN);
        drv_write_reg((drv_id_t)i, DRV_REG_IC1, v);

        // Clear any power-on faults
        drv_write_reg((drv_id_t)i, DRV_REG_IC3,
                      (uint8_t)(DRV_CLR_FLT |
                                (DRV_LOCK_UNLOCK << DRV_LOCK_SHIFT)));
    }

    log_hex("DRV8873 present mask", present);
    if (present == 0) {
        log_error("no DRV8873 responded");
        return DRV_ERR_DEAD;
    }
    return DRV_OK;
}
