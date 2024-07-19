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

# Set the PICO_BOARD if it's not already set (allow a board to override it).
if(NOT PICO_BOARD)
    string(TOLOWER ${MICROPY_BOARD} PICO_BOARD)
endif()

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

set(PICOLIBC_SYSROOT ${CMAKE_CURRENT_SOURCE_DIR}/build-picolibc/install)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${PICOLIBC_SYSROOT}/include -ftls-model=local-exec")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${PICOLIBC_SYSROOT}/lib")

add_subdirectory(freertos)

set(MICROPY_FREERTOS ON)
set(MICROPY_DYNLINK OFF)
set(MICROPY_PY_AUDIO_MP3 OFF)
set(MICROPY_LVGL OFF)
set(MICROPY_SSL_MBEDTLS OFF)
include(cmake/micropy.cmake)
