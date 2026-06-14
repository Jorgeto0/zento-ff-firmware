# =============================================================================
# mcu2_slave/CMakeLists.txt — MCU2 Build Configuration
# Target: RP2350B — Slave role
# Mirrors MCU1 config — hardware FPU enabled
# =============================================================================

cmake_minimum_required(VERSION 3.17)

# -----------------------------------------------------------------------------
# Executable — all MCU2 source files listed here
# Add new .c files here as they are created
# -----------------------------------------------------------------------------
add_executable(mcu2_slave
    main.c
    diagnostics/uart_log.c
)

# -----------------------------------------------------------------------------
# Hardware floating point — same as MCU1
# -----------------------------------------------------------------------------
target_link_libraries(mcu2_slave
    pico_stdlib
    hardware_uart
    hardware_watchdog
    pico_float_pico
    pico_double_pico
)

# -----------------------------------------------------------------------------
# Include paths — same pattern as MCU1
# -----------------------------------------------------------------------------
target_include_directories(mcu2_slave PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/shared
)

# -----------------------------------------------------------------------------
# UART diagnostics only — no USB on MCU2
# MCU2 never talks to PC directly — all comms go through MCU1
# -----------------------------------------------------------------------------
pico_enable_stdio_usb(mcu2_slave 0)
pico_enable_stdio_uart(mcu2_slave 1)

# -----------------------------------------------------------------------------
# Generate UF2 flash file for MCU2
# -----------------------------------------------------------------------------
pico_add_extra_outputs(mcu2_slave)
