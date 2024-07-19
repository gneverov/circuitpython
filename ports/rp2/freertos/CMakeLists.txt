cmake_minimum_required(VERSION 3.13)

add_subdirectory(${MICROPY_DIR}/lib/freertos/portable/ThirdParty/GCC/RP2040 lib/freertos)

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
