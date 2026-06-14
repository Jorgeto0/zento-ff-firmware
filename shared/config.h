#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// shared/config.h — Pin Definitions (Single Source of Truth)
// Hardware: ZENTO FF — RP2350B
// Schematic: v1 (single MCU) — dual MCU schematic pending
// Last updated: Phase 1
// =============================================================================
// ⚠️  All TBD pins set to 0xFF until updated schematic arrives
// ⚠️  Never hardcode GPIO numbers outside this file
// =============================================================================


// -----------------------------------------------------------------------------
// MCU1 — UART (Diagnostics)
// -----------------------------------------------------------------------------
#define UART0_TX_PIN        0   // GPIO0 — ESP_TX (shared with ESP8684)
#define UART0_RX_PIN        1   // GPIO1 — ESP_RX (shared with ESP8684)


// -----------------------------------------------------------------------------
// MCU1 — I2C Buses
// -----------------------------------------------------------------------------
#define I2C1_SDA_PIN        2   // GPIO2  — Front Left stick (TMAG5273 + PCAL6416A)
#define I2C1_SCL_PIN        3   // GPIO3  — Front Left stick
#define I2C0_SDA_PIN        4   // GPIO4  — Front Right stick + MCP9808 temp sensors
#define I2C0_SCL_PIN        5   // GPIO5  — Front Right stick + MCP9808 temp sensors


// -----------------------------------------------------------------------------
// MCU1 — Motor PWM + DIR (MAX14870 drivers)
// ⚠️  EN_COIL1 controls BOTH COIL1 and COIL2 together
// -----------------------------------------------------------------------------
#define PWM_COIL1_PIN       6   // GPIO6
#define DIR_COIL1_PIN       7   // GPIO7
#define PWM_COIL2_PIN       8   // GPIO8
#define DIR_COIL2_PIN       9   // GPIO9
#define PWM_COIL3_PIN       10  // GPIO10
#define DIR_COIL3_PIN       11  // GPIO11
#define PWM_COIL4_PIN       12  // GPIO12
#define DIR_COIL4_PIN       13  // GPIO13
#define PWM_COIL5_PIN       14  // GPIO14
#define DIR_COIL5_PIN       15  // GPIO15
#define PWM_COIL6_PIN       16  // GPIO16
#define DIR_COIL6_PIN       17  // GPIO17
#define PWM_COIL7_PIN       28  // GPIO28
#define DIR_COIL7_PIN       29  // GPIO29
#define PWM_COIL8_PIN       30  // GPIO30
#define DIR_COIL8_PIN       31  // GPIO31
#define PWM_COIL9_PIN       32  // GPIO32
#define DIR_COIL9_PIN       33  // GPIO33
#define PWM_COIL10_PIN      34  // GPIO34
#define DIR_COIL10_PIN      35  // GPIO35


// -----------------------------------------------------------------------------
// MCU1 — EN (Enable) + FAULT pins — TBD (not labeled in v1 schematic)
// ⚠️  EN_COIL1 enables BOTH COIL1 and COIL2
// -----------------------------------------------------------------------------
#define EN_COIL1_PIN        0xFF  // TBD — controls COIL1 + COIL2 together
#define EN_COIL2_PIN        0xFF  // TBD — controls COIL3
#define EN_COIL3_PIN        0xFF  // TBD — controls COIL4
#define EN_COIL4_PIN        0xFF  // TBD — controls COIL5
#define EN_COIL5_PIN        0xFF  // TBD — controls COIL6
#define EN_COIL6_PIN        0xFF  // TBD — controls COIL7
#define EN_COIL7_PIN        0xFF  // TBD — controls COIL8
#define EN_COIL8_PIN        0xFF  // TBD — controls COIL9
#define EN_COIL9_PIN        0xFF  // TBD — controls COIL10

#define FAULT_COIL1_PIN     0xFF  // TBD
#define FAULT_COIL2_PIN     0xFF  // TBD
#define FAULT_COIL3_PIN     0xFF  // TBD
#define FAULT_COIL4_PIN     0xFF  // TBD
#define FAULT_COIL5_PIN     0xFF  // TBD
#define FAULT_COIL6_PIN     0xFF  // TBD
#define FAULT_COIL7_PIN     0xFF  // TBD
#define FAULT_COIL8_PIN     0xFF  // TBD
#define FAULT_COIL9_PIN     0xFF  // TBD


// -----------------------------------------------------------------------------
// MCU1 — SPI Bus (MCP3208 ADC + LSM6DSL IMU + TFT Display)
// -----------------------------------------------------------------------------
#define SPI_CLK_PIN         18  // GPIO18
#define SPI_TX_PIN          19  // GPIO19 — MOSI
#define SPI_RX_PIN          20  // GPIO20 — MISO
#define CS_DISPLAY_PIN      21  // GPIO21 — TFT display
#define CS_IMU_PIN          22  // GPIO22 — LSM6DSL IMU
#define CS_MCP3208_PIN      0xFF // TBD — not labeled in v1 schematic


// -----------------------------------------------------------------------------
// MCU1 — Buttons (direct GPIO)
// -----------------------------------------------------------------------------
#define SW1_PIN             23  // GPIO23
#define SW2_PIN             24  // GPIO24
#define SW3_PIN             25  // GPIO25
#define SW4_PIN             26  // GPIO26
#define SW5_PIN             27  // GPIO27


// -----------------------------------------------------------------------------
// MCU1 — Misc GPIO
// -----------------------------------------------------------------------------
#define RGB_R_PIN           36  // GPIO36 — RGB LED red / BOOT
#define ESP_RST_PIN         37  // GPIO37 — ESP8684 reset
#define WHEEL1_PIN          38  // GPIO38 — Rotary encoder 1
#define WHEEL2_PIN          39  // GPIO39 — Rotary encoder 2


// -----------------------------------------------------------------------------
// MCU1 — ADC Pins
// Coils 1-8 current → INA240 → MCP3208 SPI ADC (not direct ADC)
// Coils 9-10 current → INA240 → direct GPIO ADC
// -----------------------------------------------------------------------------
#define ADC_COIL9_PIN       40  // GPIO40 — ADC0 — INA240 coil 9 direct
#define ADC_COIL10_PIN      41  // GPIO41 — ADC1 — INA240 coil 10 direct
#define VOLTAGE_SENS_PIN    42  // GPIO42 — ADC2 — voltage monitoring
// GPIO43-47 ADC3-7 — TAHO + others TBD


// -----------------------------------------------------------------------------
// MCU1 — PIO Inter-MCU Bus — TBD (waiting for dual MCU schematic)
// -----------------------------------------------------------------------------
#define PIO_BUS_PIN_0       0xFF  // TBD
#define PIO_BUS_PIN_1       0xFF  // TBD
#define PIO_BUS_PIN_2       0xFF  // TBD
#define PIO_BUS_PIN_3       0xFF  // TBD


// =============================================================================
// MCU2 — All pins TBD (updated schematic pending)
// =============================================================================

// MCU2 — I2C (Stick 2 position + buttons)
#define MCU2_I2C_SDA_PIN    0xFF  // TBD
#define MCU2_I2C_SCL_PIN    0xFF  // TBD

// MCU2 — Motor PWM + DIR (coil split TBD)
#define MCU2_PWM_COIL1_PIN  0xFF  // TBD
#define MCU2_DIR_COIL1_PIN  0xFF  // TBD
#define MCU2_PWM_COIL2_PIN  0xFF  // TBD
#define MCU2_DIR_COIL2_PIN  0xFF  // TBD
#define MCU2_PWM_COIL3_PIN  0xFF  // TBD
#define MCU2_DIR_COIL3_PIN  0xFF  // TBD
#define MCU2_PWM_COIL4_PIN  0xFF  // TBD
#define MCU2_DIR_COIL4_PIN  0xFF  // TBD
#define MCU2_PWM_COIL5_PIN  0xFF  // TBD
#define MCU2_DIR_COIL5_PIN  0xFF  // TBD

// MCU2 — EN + FAULT pins
#define MCU2_EN_COIL1_PIN   0xFF  // TBD
#define MCU2_FAULT_COIL1_PIN 0xFF // TBD

// MCU2 — ADC (current sensing — split TBD)
#define MCU2_ADC_PIN_0      0xFF  // TBD

// MCU2 — PIO Bus (slave side)
#define MCU2_PIO_BUS_PIN_0  0xFF  // TBD
#define MCU2_PIO_BUS_PIN_1  0xFF  // TBD
#define MCU2_PIO_BUS_PIN_2  0xFF  // TBD
#define MCU2_PIO_BUS_PIN_3  0xFF  // TBD

// MCU2 — UART Diagnostics
#define MCU2_UART_TX_PIN    0xFF  // TBD
#define MCU2_UART_RX_PIN    0xFF  // TBD


#endif // CONFIG_H
