#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// shared/config.h — Pin Definitions (Single Source of Truth)
// Hardware: ZENTO Flux — Controller_Board_V1 — Dual RP2350B
// MCU1 = U2  — MainController-LEFT   (USB HID, display, coils 1-4 + VC1)
// MCU2 = U11 — SlaveController-RIGHT (coils 1-4 + VC1, right side)
// Source: Netlist_Schematic3_1_2026-08-20.enet — extracted, not transcribed
// =============================================================================
// V1 vs V0: DRV8873SPWPR SPI H-bridges replace MAX14870. MCP3208 removed —
// current sense goes direct to MCU ADC. Fault lines moved to PCAL6416A (I2C).
// =============================================================================

// =============================================================================
// MCU1 — MainController-LEFT (U2)
// =============================================================================

// PIO inter-MCU bus — same GPIOs on both chips, 27.4R series terminated
#define MCU1_PIO1_PIN            1   // pin 78
#define MCU1_PIO2_PIN            2   // pin 79
#define MCU1_PIO3_PIN            3   // pin 80
#define MCU1_PIO4_PIN            4   // pin 1
#define MCU1_PIO_CLK_PIN         5   // pin 2

// Display SPI
#define SPI_DISPLAY_MOSI_PIN     6   // pin 3
#define SPI_DISPLAY_MISO_PIN     7   // pin 4
#define SPI_DISPLAY_SCK_PIN      8   // pin 6
#define SPI_DISPLAY_CS_PIN       9   // pin 7
// SPI_DISPLAY_RST is on the PCAL6416A expander (P0_4), not a GPIO

// I2C
#define M_I2C1_SDA_PIN          10   // pin 8
#define M_I2C1_SCL_PIN          11   // pin 9
#define M_I2C0_SDA_PIN          12   // pin 11
#define M_I2C0_SCL_PIN          13   // pin 12

// Sensors
#define M_SPI1_CS_AS_PIN        14   // pin 13 — AS5047 angle sensor CS
#define INT_EXPANDER_PIN        15   // pin 14 — PCAL6416A interrupt (input)

// SPI0 — TMAG hall sensor
#define M_SPI0_SDI_PIN          16   // pin 16
#define M_SPI0_CS_PIN           17   // pin 17
#define M_SPI0_SCK_PIN          18   // pin 18
#define M_SPI0_SDO_PIN          19   // pin 19

// SPI1 — LSM6DSL gyro
#define M_SPI1_SDI_PIN          20   // pin 20
#define M_SPI1_CS_PIN           21   // pin 21
#define M_SPI1_SCK_PIN          22   // pin 22
#define M_SPI1_SDO_PIN          23   // pin 23

// DRV8873 per-driver chip selects
#define M_SPI_CS_COIL1_PIN      24   // pin 25
#define M_SPI_CS_COIL2_PIN      25   // pin 26
#define M_SPI_CS_COIL3_PIN      26   // pin 27
#define M_SPI_CS_COIL4_PIN      27   // pin 28
#define M_SPI_CS_VC1_PIN         0   // pin 77

// Motor PWM + DIR — 4 coils + 1 voice coil
#define M_PWM_COIL1_PIN         28   // pin 36
#define M_DIR_COIL1_PIN         29   // pin 37
#define M_PWM_COIL2_PIN         30   // pin 38
#define M_DIR_COIL2_PIN         31   // pin 39
#define M_PWM_COIL3_PIN         32   // pin 40
#define M_DIR_COIL3_PIN         33   // pin 42
#define M_PWM_COIL4_PIN         34   // pin 43
#define M_DIR_COIL4_PIN         35   // pin 44
#define M_PWM_VC1_PIN           36   // pin 45
#define M_DIR_VC1_PIN           37   // pin 46

// DRV8873 shared SPI bus (config/diagnostics)
#define M_SPI_DO_COIL_PIN       38   // pin 47
#define M_SPI_DI_COIL_PIN       39   // pin 48
#define M_SPI_SCK_COIL_PIN      40   // pin 49

// Status
#define M_LED_G_PIN             41   // pin 52 — the only plain LED on the board
#define SW1_PIN                 42   // pin 53
// SW2-SW5 are on the PCAL6416A expander (P0_0..P0_3), not GPIOs

// Current sense — direct to ADC (IPROPI from each DRV8873)
#define M_CURRENT_VC1_PIN       43   // pin 54 ADC3
#define M_CURRENT_COIL1_PIN     44   // pin 55 ADC4
#define M_CURRENT_COIL2_PIN     45   // pin 56 ADC5
#define M_CURRENT_COIL3_PIN     46   // pin 57 ADC6
#define M_CURRENT_COIL4_PIN     47   // pin 58 ADC7

// UART diagnostics — DEV ONLY, borrows SW1. GPIO42 muxes to UART1_TX.
// Revert to SW1 for production. TX only, no RX pin available.
#define UART0_TX_PIN            42
#define UART0_RX_PIN            0xFF

// =============================================================================
// MCU2 — SlaveController-RIGHT (U11)
// =============================================================================

#define MCU2_PIO1_PIN            1   // pin 78
#define MCU2_PIO2_PIN            2   // pin 79
#define MCU2_PIO3_PIN            3   // pin 80
#define MCU2_PIO4_PIN            4   // pin 1
#define MCU2_PIO_CLK_PIN         5   // pin 2

// DRV8873 per-driver chip selects
#define S_SPI_CS_COIL1_PIN       6   // pin 3
#define S_SPI_CS_COIL2_PIN       7   // pin 4
#define S_SPI_CS_COIL3_PIN       8   // pin 6
#define S_SPI_CS_COIL4_PIN       9   // pin 7
#define S_SPI_CS_VC1_PIN        24   // pin 25

// I2C
#define S_I2C1_SDA_PIN          10   // pin 8
#define S_I2C1_SCL_PIN          11   // pin 9
#define S_I2C0_SDA_PIN          12   // pin 11
#define S_I2C0_SCL_PIN          13   // pin 12

// Encoders
#define WHEEL_1_PIN             14   // pin 13
#define WHEEL_2_PIN             15   // pin 14

// SPI0 — TMAG hall sensor
#define S_SPI0_SDI_PIN          16   // pin 16
#define S_SPI0_CS_PIN           17   // pin 17
#define S_SPI0_SCK_PIN          18   // pin 18
#define S_SPI0_SDO_PIN          19   // pin 19

// SPI1 — AS5047 angle sensor
#define S_SPI1_SDI_PIN          20   // pin 20
#define S_SPI1_CS_PIN           21   // pin 21
#define S_SPI1_SCK_PIN          22   // pin 22
#define S_SPI1_SDO_PIN          23   // pin 23

// Motor PWM + DIR
#define S_PWM_COIL1_PIN         25   // pin 26
#define S_DIR_COIL1_PIN         26   // pin 27
#define S_PWM_COIL2_PIN         27   // pin 28
#define S_DIR_COIL2_PIN         28   // pin 36
#define S_PWM_COIL3_PIN         29   // pin 37
#define S_DIR_COIL3_PIN         30   // pin 38
#define S_PWM_COIL4_PIN         31   // pin 39
#define S_DIR_COIL4_PIN         32   // pin 40
#define S_PWM_VC1_PIN           33   // pin 42
#define S_DIR_VC1_PIN           34   // pin 43

// DRV8873 shared SPI bus
#define S_SPI_DO_COIL_PIN       37   // pin 46
#define S_SPI_DI_COIL_PIN       38   // pin 47
#define S_SPI_SCK_COIL_PIN      39   // pin 48

// Analog + RGB
#define VOLTAGE_SENS_PIN        40   // pin 49 ADC0
#define RGB_L_PIN               41   // pin 52 — addressable, needs driver
#define RGB_R_PIN                0   // pin 77 — addressable, needs driver
#define TAHO_PIN                42   // pin 53

// Current sense — direct to ADC
#define S_CURRENT_VC1_PIN       43   // pin 54 ADC3
#define S_CURRENT_COIL1_PIN     44   // pin 55 ADC4
#define S_CURRENT_COIL2_PIN     45   // pin 56 ADC5
#define S_CURRENT_COIL3_PIN     46   // pin 57 ADC6
#define S_CURRENT_COIL4_PIN     47   // pin 58 ADC7

// Free pins — GPIO35 (pin 44) and GPIO36 (pin 45) are unconnected on U11.
// GPIO36 muxes to UART1_TX, so diagnostics here cost nothing.
#define MCU2_UART_TX_PIN        36
#define MCU2_UART_RX_PIN        0xFF
#define MCU2_SPARE_PIN          35

// MCU2 has NO plain LED — only addressable RGB. No simple heartbeat possible.

#endif // CONFIG_H
