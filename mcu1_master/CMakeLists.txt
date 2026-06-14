# =============================================================================
# mcu1_master/CMakeLists.txt — MCU1 Build Configuration
# Target: RP2350B — Master role
# =============================================================================

cmake_minimum_required(VERSION 3.17)

# -----------------------------------------------------------------------------
# Executable — all MCU1 source files listed here
# Add new .c files here as they are created
# -----------------------------------------------------------------------------
add_executable(mcu1_master
    main.c
    diagnostics/uart_log.c
)

# -----------------------------------------------------------------------------
# Hardware floating point — RP2350B has a real FPU
# pico_float_pico   = use hardware FPU for float
# pico_double_pico  = use hardware FPU for double
# Never use software float on this chip — it's 10-20x slower
# -----------------------------------------------------------------------------
target_link_libraries(mcu1_master
    pico_stdlib
    hardware_uart
    hardware_watchdog
    pico_float_pico
    pico_double_pico
)

# -----------------------------------------------------------------------------
# Include paths — compiler looks here for #include files
# shared/ is included so both MCUs can reach config.h, crc.h, protocol.h
# -----------------------------------------------------------------------------
target_include_directories(mcu1_master PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/shared
)

# -----------------------------------------------------------------------------
# USB output disabled — we use UART for diagnostics in Phase 1
# stdout goes to UART0, not USB
# This changes in Phase 1.3 when TinyUSB HID stack is added
# -----------------------------------------------------------------------------
pico_enable_stdio_usb(mcu1_master 0)
pico_enable_stdio_uart(mcu1_master 1)

# -----------------------------------------------------------------------------
# Generate UF2 flash file — this is what gets dragged onto the RP2350B
# Also generates .bin and .hex for other flash methods
# -----------------------------------------------------------------------------
pico_add_extra_outputs(mcu1_master)
