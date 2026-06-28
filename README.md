# ZENTO FF — Haptic Force Feedback Joystick Controller

> High-performance haptic joystick firmware for sim racing and flight simulation, built on dual RP2350B microcontrollers with 10 independent electromagnetic force feedback channels.

---

## Overview

The ZENTO FF is a custom-designed haptic force feedback joystick controller built from the ground up — PCB design in EasyEDA through to production-ready embedded firmware in C. The system delivers real-time, closed-loop force feedback with USB HID connectivity at 1000Hz polling, designed for direct integration with SimHub.

The firmware runs on two RP2350B microcontrollers communicating over a custom PIO-based synchronous serial bus, coordinating 10 independent haptic coil channels to produce precise, low-latency force sensations across two joystick axes.

---

## Hardware Architecture

### Microcontrollers

| Unit | Chip | Role |
|---|---|---|
| MCU1 (Master) | RP2350B | USB HID, display, Stick 1, PIO bus master |
| MCU2 (Slave) | RP2350B | Stick 2 processing, PIO bus slave |

### Key ICs

| Component | Qty | Purpose | Interface |
|---|---|---|---|
| RP2350B | 2 | Main microcontrollers | — |
| MAX14870ETC+ | 10 | Full-bridge motor drivers | PWM + DIR + EN |
| INA240A3PWR | 10 | High-side current sense amplifiers | Analog |
| MCP3208T | 1 | 8-channel 12-bit SPI ADC | SPI |
| TMAG5273A1 | 2 | 3D magnetic position sensors | I2C |
| PCAL6416AHF | 2 | GPIO expanders (buttons + encoders) | I2C |
| MCP9808T | 2 | Digital temperature sensors | I2C |
| LSM6DSLTR | 1 | 6-axis IMU | SPI |
| ESP8684-MINI-1 | 1 | WiFi module | UART |
| TFT 240×240 | 1 | Display | SPI |
| TPS566235 | 2 | Buck converters (6V motor rails) | — |
| W25Q16 | 1 | External SPI Flash | QSPI |
| APS6404L | 1 | External PSRAM | QSPI |

### Power Rails

| Rail | Source | Powers |
|---|---|---|
| 12–24V | DC barrel jack or USB-C PD | System input |
| 6V | TPS566235 buck (×2) | All 10 motor drivers |
| 3.3V | LDO | MCUs, sensors, logic |

---

## Firmware Features

- **Dual-MCU architecture** — MCU1 and MCU2 each handle one joystick axis independently
- **Custom PIO bus** — 1MHz synchronous serial inter-MCU link using RP2350B PIO state machines, with CRC8 packet validation
- **USB HID at 1000Hz** — reports dual joystick axes, buttons, and 10-channel current telemetry to the host PC
- **Closed-loop PID current control** — per-coil current regulation via INA240 sense amplifiers and MCP3208 SPI ADC
- **Hardware FPU** — floating-point PID math runs on the RP2350B's hardware FPU, not software emulation
- **Watchdog timers** — 500ms hardware watchdog on both MCUs with debug pause support
- **Zero-allocation UART diagnostics** — fixed-buffer severity-level logging (INFO / WARNING / ERROR) on both MCUs
- **Full pin abstraction** — all GPIO assignments live in a single `config.h`, nothing is hardcoded

---

## Firmware Architecture

```
zento-ff-firmware/
├── CMakeLists.txt
├── shared/
│   ├── config.h          ← All GPIO pin definitions (single source of truth)
│   ├── protocol.h        ← PIO bus packet format
│   └── crc.h             ← CRC8/SMBUS engine (flash lookup table)
├── mcu1_master/
│   ├── main.c
│   ├── tusb_config.h
│   ├── usb_hid/
│   │   ├── hid_descriptors.h/c
│   │   └── hid.h/c
│   ├── pio_bus/
│   │   ├── bus_master.pio
│   │   └── pio_master.h/c
│   └── diagnostics/
│       └── uart_log.h/c
└── mcu2_slave/
    ├── main.c
    ├── pio_bus/
    │   ├── bus_slave.pio
    │   └── pio_slave.h/c
    └── diagnostics/
        └── uart_log.h/c
```

---

## USB HID Report

The primary HID endpoint reports at **1000Hz** and contains:

| Field | Type | Description |
|---|---|---|
| Stick 1 X | int16 | Axis — magnetic position sensor |
| Stick 1 Y | int16 | Axis — magnetic position sensor |
| Stick 2 X | int16 | Axis — from MCU2 via PIO bus |
| Stick 2 Y | int16 | Axis — from MCU2 via PIO bus |
| Buttons | uint8 | 8 button states |
| Coil currents | int16 ×10 | Per-channel current telemetry |

---

## Inter-MCU PIO Bus

MCU1 and MCU2 are connected by a custom 1-bit synchronous serial bus implemented entirely in RP2350B PIO state machines — no CPU involvement during transfer.

- **Speed:** 1MHz clock
- **Direction:** MCU1 initiates, MCU2 responds
- **Packet format:** `[START][LENGTH][PAYLOAD][CRC8]`
- **Validation:** CRC8/SMBUS with lookup table in flash
- **Latency:** Sub-millisecond round trip

**MCU1 → MCU2:** Force vectors for Stick 2 (5 coil channels) + sync timestamp  
**MCU2 → MCU1:** Stick 2 position (X/Y from TMAG5273) + current readings + status flags

---

## Build Instructions

### Requirements

- Raspberry Pi Pico SDK (RP2350B support required)
- `arm-none-eabi-gcc`
- CMake 3.13+

### Setup

```bash
# Clone with SDK path configured
git clone https://github.com/Jorgeto0/zento-ff-firmware.git
cd zento-ff-firmware

export PICO_SDK_PATH=/path/to/pico-sdk

mkdir build && cd build
cmake .. -DPICO_BOARD=pico2
make -j4
```

### Output

```
build/mcu1_master/mcu1_master.uf2   ← Flash to MCU1
build/mcu2_slave/mcu2_slave.uf2     ← Flash to MCU2
```

Hold BOOTSEL, connect USB, drag and drop the `.uf2` file onto the drive.

---

## PCB Design

Schematic and PCB layout designed in **EasyEDA Pro**. The design includes:

- Main controller board (MCU1 — RP2350B)
- Slave controller board (MCU2 — RP2350B)
- Front Left stick board (TMAG5273 + PCAL6416A)
- Front Right stick board (TMAG5273 + PCAL6416A)
- Power board (USB-C PD, buck converters, motor driver rails)

---

## Tech Stack

`C11` · `RP2350B` · `Pico SDK` · `TinyUSB` · `PIO Assembly` · `EasyEDA Pro` · `CMake` · `arm-none-eabi-gcc`

---

## License

Private project — all rights reserved.
