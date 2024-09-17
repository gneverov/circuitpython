cmake_minimum_required(VERSION 3.27)
cmake_policy(SET CMP0126 OLD)

# Set build type to reduce firmware size
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

# Set main target and component locations
set(MICROPY_TARGET firmware)
get_filename_component(MICROPY_DIR "../.." ABSOLUTE)

# Set the location of this port's directory.
set(MICROPY_PORT_DIR ${CMAKE_CURRENT_LIST_DIR})

# Set the board if it's not already set.
if(NOT MICROPY_BOARD)
    set(MICROPY_BOARD RPI_PICO)
endif()

# Set the board directory and check that it exists.
if(NOT MICROPY_BOARD_DIR)
    set(MICROPY_BOARD_DIR ${MICROPY_PORT_DIR}/boards/${MICROPY_BOARD})
endif()
get_filename_component(MICROPY_BOARD_DIR ${MICROPY_BOARD_DIR} ABSOLUTE)
if(NOT EXISTS ${MICROPY_BOARD_DIR}/mpconfigboard.cmake)
    message(FATAL_ERROR "Invalid MICROPY_BOARD specified: ${MICROPY_BOARD}")
endif()

# Include board config, it may override MICROPY_FROZEN_MANIFEST
include(${MICROPY_BOARD_DIR}/mpconfigboard.cmake)


include(${MICROPY_DIR}/lib/pico-sdk/pico_sdk_init.cmake)

# Check that the compiler supports TLS, which is required for multicore Picolibc.
execute_process(
    COMMAND ${CMAKE_C_COMPILER} -v
    ERROR_VARIABLE C_COMPILER_VERSION
)
if(C_COMPILER_VERSION MATCHES --disable-tls)
    message(FATAL_ERROR "C compiler '${CMAKE_C_COMPILER}' does not support TLS")
endif()

find_package(Python3 COMPONENTS Interpreter)

# Helper function to remove a list items from a target property.
function(target_remove_property_value target property)
    get_target_property(props ${target} ${property})
    list(REMOVE_ITEM props ${ARGN})
    set_target_properties(${target} PROPERTIES ${property} "${props}")
endfunction()

# Define the top-level project
project(${MICROPY_TARGET})

# Calls pico_sdk_init()
include("cmake/pico-sdk.cmake")

if(NOT EXISTS ${MICROPY_PORT_DIR}/chips/${PICO_CHIP})
    message(FATAL_ERROR "MicroPython does not support ${PICO_CHIP} chip")
endif()
include(${MICROPY_PORT_DIR}/chips/${PICO_CHIP}/chip.cmake)

# Find the location of picolibc based on the location of GCC
if(NOT DEFINED PICOLIBC_SYSROOT)
    set(PICOLIBC_SYSROOT ${CMAKE_C_COMPILER})
    cmake_path(GET PICOLIBC_SYSROOT PARENT_PATH PICOLIBC_SYSROOT)
    cmake_path(APPEND PICOLIBC_SYSROOT .. picolibc)
    cmake_path(NORMAL_PATH PICOLIBC_SYSROOT)
endif()
if(NOT EXISTS ${PICOLIBC_SYSROOT})
    message(FATAL_ERROR "Picolibc not found at ${PICOLIBC_SYSROOT}")
endif()
if(NOT EXISTS ${PICOLIBC_SYSROOT}/lib/${PICOLIBC_BUILDTYPE})
    message(FATAL_ERROR "Picolibc build type ${PICOLIBC_BUILDTYPE} not found in ${PICOLIBC_SYSROOT}")
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${PICOLIBC_SYSROOT}/include -ftls-model=local-exec")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostartfiles -L${PICOLIBC_SYSROOT}/lib/${PICOLIBC_BUILDTYPE}")

add_subdirectory(freertos)

set(MICROPY_FREERTOS ON)
set(MICROPY_DYNLINK ON)
set(MICROPY_PY_AUDIO_MP3 ON)
set(MICROPY_LVGL ON)
set(MICROPY_SSL_MBEDTLS OFF)
include(cmake/micropy.cmake)
