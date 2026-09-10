cmake_minimum_required(VERSION 3.17)

# Must be set before project() call
set(PICO_BOARD pico2)

# This board uses the RP2350B, 48 GPIOs, not the 30-pin RP2350A that the
# pico2 board file assumes. Without this the SDK masks bit 5 off every PIO
# pin number, so GPIO38/39/40 silently become GPIO6/7/8.
# See hardware/pio.h: "If PICO_PIO_USE_GPIO_BASE != 1 then bit 5 of the pin
# number is ignored... defaulted to 0 if PICO_RP2350A is 1"
set(PICO_RP2350A 0)
add_compile_definitions(PICO_PIO_USE_GPIO_BASE=1)

# Pull in Pico SDK (must be before project())
include(pico_sdk_import.cmake)

project(zento-ff-firmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Build both MCUs
add_subdirectory(mcu1_master)
add_subdirectory(mcu2_slave)
