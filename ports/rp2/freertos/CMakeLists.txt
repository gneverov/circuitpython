cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED FREERTOS_DIR)
    message(FATAL_ERROR "FREERTOS_DIR not defined")
endif()
if(NOT EXISTS ${FREERTOS_DIR})
    message(FATAL_ERROR "FREERTOS_DIR ${FREERTOS_DIR} not found")
endif()

add_subdirectory(${FREERTOS_DIR} lib/freertos)

add_library(freertos INTERFACE)

target_sources(freertos INTERFACE
    heap_malloc.c
    interrupts.c
    port_hooks.c
)

target_include_directories(freertos INTERFACE
    include
)

target_link_libraries(freertos INTERFACE
    FreeRTOS-Kernel
)
