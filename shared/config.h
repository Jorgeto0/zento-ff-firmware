#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// shared/config.h — Pin Definitions (Single Source of Truth)
// Hardware: ZENTO FF — Dual RP2350B
// MCU1 = U2  — MainController-LEFT  (USB HID, display, coils 1-4 + 10)
// MCU2 = U11 — SlaveController-RIGHT (coils 5-9, right stick)
// Verified: 2026-06-28 from EasyEDA schematics
// =============================================================================


// =============================================================================
// MCU1 — MainController-LEFT (U2)
// =============================================================================

// -----------------------------------------------------------------------------
// MCU1 — Buttons
// -----------------------------------------------------------------------------
#define SW1_PIN                 0   // GPIO0

// -----------------------------------------------------------------------------
// MCU1 — PIO Inter-MCU Bus
// ⚠️  PIO1-PIO4 = data lines, PIO_CLK = clock
// ⚠️  Consecutive pin constraint: PIO_CLK must = last PIO data pin + 1
// -----------------------------------------------------------------------------
#define MCU1_PIO1_PIN           1   // GPIO1
#define MCU1_PIO2_PIN           2   // GPIO2
#define MCU1_PIO3_PIN           3   // GPIO3
#define MCU1_PIO4_PIN           4   // GPIO4
#define MCU1_PIO_CLK_PIN        5   // GPIO5

// -----------------------------------------------------------------------------
// MCU1 — SPI Display
// -----------------------------------------------------------------------------
#define SPI_DISPLAY_MOSI_PIN    6   // GPIO6
#define SPI_DISPLAY_MISO_PIN    7   // GPIO7
#define SPI_DISPLAY_SCK_PIN     8   // GPIO8
#define SPI_DISPLAY_CS_PIN      9   // GPIO9

// -----------------------------------------------------------------------------
// MCU1 — I2C Buses
// -----------------------------------------------------------------------------
#define M_I2C1_SDA_PIN          10  // GPIO10 — Left stick TMAG + PCAL6416A
#define M_I2C1_SCL_PIN          11  // GPIO11
#define M_I2C0_SDA_PIN          12  // GPIO12 — Temp sensor + right stick
#define M_I2C0_SCL_PIN          13  // GPIO13

// -----------------------------------------------------------------------------
// MCU1 — Misc GPIO
// -----------------------------------------------------------------------------
#define RGB_L_PIN               14  // GPIO14 — RGB LED left thumbstick
#define TAHO_PIN                15  // GPIO15 — Tachometer signal

// -----------------------------------------------------------------------------
// MCU1 — SPI0 Bus (TMAG5170 Hall sensor)
// -----------------------------------------------------------------------------
#define M_SPI0_SDI_PIN          16  // GPIO16 — MISO
#define M_SPI0_CS_PIN           17  // GPIO17 — Chip select
#define M_SPI0_SCK_PIN          18  // GPIO18 — Clock
#define M_SPI0_SDO_PIN          19  // GPIO19 — MOSI

// -----------------------------------------------------------------------------
// MCU1 — SPI1 Bus (LSM6DSL Gyro + AS5047 angle sensor)
// -----------------------------------------------------------------------------
#define M_SPI1_SDI_PIN          20  // GPIO20 — MISO
#define M_SPI1_CS_PIN           21  // GPIO21 — Chip select (GYRO)
#define M_SPI1_SCK_PIN          22  // GPIO22 — Clock
#define M_SPI1_SDO_PIN          23  // GPIO23 — MOSI

// -----------------------------------------------------------------------------
// MCU1 — Motor PWM + DIR
// Coils 1-4 on MCU1, Coil 10 also on MCU1
// ⚠️  Coils 5-9 are on MCU2
// ⚠️  Note gap in pin numbers — GPIO28 skips to pin 36 (chip layout)
// -----------------------------------------------------------------------------
#define PWM_COIL1_PIN           24  // GPIO24 — pin 25
#define DIR_COIL1_PIN           25  // GPIO25 — pin 26
#define PWM_COIL2_PIN           26  // GPIO26 — pin 27
#define DIR_COIL2_PIN           27  // GPIO27 — pin 28
#define PWM_COIL3_PIN           28  // GPIO28 — pin 36
#define DIR_COIL3_PIN           29  // GPIO29 — pin 37
#define PWM_COIL4_PIN           30  // GPIO30 — pin 38
#define DIR_COIL4_PIN           31  // GPIO31 — pin 39
#define PWM_COIL10_PIN          32  // GPIO32 — pin 40
#define DIR_COIL10_PIN          33  // GPIO33 — pin 42

// -----------------------------------------------------------------------------
// MCU1 — EN + FAULT pins
// ⚠️  EN_COIL_THUMB_LEFT  = enable for thumb stick coils (left side)
// ⚠️  EN_COIL_COIL_LEFT   = enable for regular coils (left side)
// -----------------------------------------------------------------------------
#define EN_COIL_THUMB_LEFT_PIN  34  // GPIO34 — pin 43
#define FAULT_THUMB_LEFT_PIN    35  // GPIO35 — pin 44
#define EN_COIL_COIL_LEFT_PIN   36  // GPIO36 — pin 45
#define FAULT_COIL_LEFT_PIN     37  // GPIO37 — pin 46

// -----------------------------------------------------------------------------
// MCU1 — LEDs
// -----------------------------------------------------------------------------
#define M_LED_G_PIN             38  // GPIO38 — pin 47
#define M_LED_R_PIN             39  // GPIO39 — pin 48

// -----------------------------------------------------------------------------
// MCU1 — ADC (Direct ADC — thumb stick current channels 1-4 + 10)
// -----------------------------------------------------------------------------
#define ADC_THUMB_1_PIN         40  // GPIO40 ADC0 — pin 49
#define ADC_THUMB_2_PIN         41  // GPIO41 ADC1 — pin 52
#define ADC_THUMB_3_PIN         42  // GPIO42 ADC2 — pin 53
#define ADC_THUMB_4_PIN         43  // GPIO43 ADC3 — pin 54
#define ADC_THUMB_10_PIN        44  // GPIO44 ADC4 — pin 55

// -----------------------------------------------------------------------------
// MCU1 — Buttons (continued)
// -----------------------------------------------------------------------------
#define SW2_PIN                 45  // GPIO45 ADC5 — pin 56
#define SW3_PIN                 46  // GPIO46 ADC6 — pin 57

// -----------------------------------------------------------------------------
// MCU1 — AS5047 angle sensor chip select
// -----------------------------------------------------------------------------
#define M_SPI1_CS_AS_PIN        47  // GPIO47 ADC7 — pin 58


// =============================================================================
// MCU2 — SlaveController-RIGHT (U11)
// =============================================================================

// -----------------------------------------------------------------------------
// MCU2 — Buttons
// -----------------------------------------------------------------------------
#define SW4_PIN                 0   // GPIO0 — pin 77

// -----------------------------------------------------------------------------
// MCU2 — PIO Inter-MCU Bus (mirrors MCU1 exactly)
// ⚠️  Same GPIO numbers on both chips — connected via board connector
// -----------------------------------------------------------------------------
#define MCU2_PIO1_PIN           1   // GPIO1 — pin 78
#define MCU2_PIO2_PIN           2   // GPIO2 — pin 79
#define MCU2_PIO3_PIN           3   // GPIO3 — pin 80
#define MCU2_PIO4_PIN           4   // GPIO4 — pin 1
#define MCU2_PIO_CLK_PIN        5   // GPIO5 — pin 2

// -----------------------------------------------------------------------------
// MCU2 — ESP8684 WiFi control (out of scope for now)
// -----------------------------------------------------------------------------
#define MCU2_ESP_RST_PIN        6   // GPIO6  — GPIO37_ESPRST
#define MCU2_ESP_BOOT_PIN       7   // GPIO7  — GPIO36_BOOT

// -----------------------------------------------------------------------------
// MCU2 — EN + FAULT (right side regular coils)
// -----------------------------------------------------------------------------
#define EN_COIL_COIL_RIGHT_PIN  8   // GPIO8  — pin 6
#define FAULT_COIL_RIGHT_PIN    9   // GPIO9  — pin 7

// -----------------------------------------------------------------------------
// MCU2 — I2C Buses
// -----------------------------------------------------------------------------
#define S_I2C1_SDA_PIN          10  // GPIO10 — pin 8
#define S_I2C1_SCL_PIN          11  // GPIO11 — pin 9
#define S_I2C0_SDA_PIN          12  // GPIO12 — pin 11
#define S_I2C0_SCL_PIN          13  // GPIO13 — pin 12

// -----------------------------------------------------------------------------
// MCU2 — Encoders
// -----------------------------------------------------------------------------
#define WHEEL_1_PIN             14  // GPIO14 — pin 13
#define WHEEL_2_PIN             15  // GPIO15 — pin 14

// -----------------------------------------------------------------------------
// MCU2 — SPI0 Bus (TMAG5170 Hall sensor right side)
// -----------------------------------------------------------------------------
#define S_SPI0_SDI_PIN          16  // GPIO16 — pin 16
#define S_SPI0_CS_PIN           17  // GPIO17 — pin 17
#define S_SPI0_SCK_PIN          18  // GPIO18 — pin 18
#define S_SPI0_SDO_PIN          19  // GPIO19 — pin 19

// -----------------------------------------------------------------------------
// MCU2 — SPI1 Bus (AS5047 angle sensor right side)
// -----------------------------------------------------------------------------
#define S_SPI1_SDI_PIN          20  // GPIO20 — pin 20
#define S_SPI1_CS_PIN           21  // GPIO21 — pin 21
#define S_SPI1_SCK_PIN          22  // GPIO22 — pin 22
#define S_SPI1_SDO_PIN          23  // GPIO23 — pin 23

// -----------------------------------------------------------------------------
// MCU2 — Motor PWM + DIR (Coils 5-9)
// ⚠️  Same gap pattern as MCU1 — GPIO28 skips to pin 36
// -----------------------------------------------------------------------------
#define PWM_COIL5_PIN           24  // GPIO24 — pin 25
#define DIR_COIL5_PIN           25  // GPIO25 — pin 26
#define PWM_COIL6_PIN           26  // GPIO26 — pin 27
#define DIR_COIL6_PIN           27  // GPIO27 — pin 28
#define PWM_COIL7_PIN           28  // GPIO28 — pin 36
#define DIR_COIL7_PIN           29  // GPIO29 — pin 37
#define PWM_COIL8_PIN           30  // GPIO30 — pin 38
#define DIR_COIL8_PIN           31  // GPIO31 — pin 39
#define PWM_COIL9_PIN           32  // GPIO32 — pin 40
#define DIR_COIL9_PIN           33  // GPIO33 — pin 42

// -----------------------------------------------------------------------------
// MCU2 — EN + FAULT (right side thumb stick)
// -----------------------------------------------------------------------------
#define EN_COIL_THUMB_RIGHT_PIN 34  // GPIO34 — pin 43
#define FAULT_THUMB_RIGHT_PIN   35  // GPIO35 — pin 44

// -----------------------------------------------------------------------------
// MCU2 — ESP8684 UART (out of scope)
// -----------------------------------------------------------------------------
#define MCU2_ESP_RX_PIN         36  // GPIO36 — pin 45
#define MCU2_ESP_TX_PIN         37  // GPIO37 — pin 46

// -----------------------------------------------------------------------------
// MCU2 — LEDs
// -----------------------------------------------------------------------------
#define S_LED_G_PIN             38  // GPIO38 — pin 47
#define S_LED_R_PIN             39  // GPIO39 — pin 48

// -----------------------------------------------------------------------------
// MCU2 — ADC (Direct ADC — thumb stick current channels 5-9)
// -----------------------------------------------------------------------------
#define ADC_THUMB_5_PIN         40  // GPIO40 ADC0 — pin 49
#define ADC_THUMB_6_PIN         41  // GPIO41 ADC1 — pin 52
#define ADC_THUMB_7_PIN         42  // GPIO42 ADC2 — pin 53
#define ADC_THUMB_8_PIN         43  // GPIO43 ADC3 — pin 54
#define ADC_THUMB_9_PIN         44  // GPIO44 ADC4 — pin 55

// -----------------------------------------------------------------------------
// MCU2 — Buttons + Misc
// -----------------------------------------------------------------------------
#define SW5_PIN                 45  // GPIO45 ADC5 — pin 56
#define VOLTAGE_SENS_PIN        46  // GPIO46 ADC6 — pin 57
#define RGB_R_PIN               47  // GPIO47 ADC7 — pin 58

// =============================================================================
// UART Diagnostic Logging
// ⚠️  GPIO0 on both MCUs is used for buttons (SW1/SW4)
// ⚠️  No dedicated UART pins available — use USB stdio or SWD for debugging
// ⚠️  UART pins set to 0xFF — revisit with client before Phase 2
// =============================================================================
#define UART0_TX_PIN            0xFF  // TBD — no free UART pins on MCU1
#define UART0_RX_PIN            0xFF  // TBD
#define MCU2_UART_TX_PIN        0xFF  // TBD — no free UART pins on MCU2
#define MCU2_UART_RX_PIN        0xFF  // TBD

#endif // CONFIG_H
