cmake_minimum_required(VERSION 3.17)

# Must be set before project() call
set(PICO_BOARD pico2)

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
