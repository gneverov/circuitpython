cmake_minimum_required(VERSION 3.13)

add_library(freertos_config INTERFACE)

target_include_directories(freertos_config INTERFACE config)


add_library(freertos_helper INTERFACE)

target_sources(freertos_helper INTERFACE
    task_helper.c
    heap_malloc.c
    port_hooks.c
)

target_include_directories(freertos_helper INTERFACE include)

target_link_libraries(freertos_helper INTERFACE
    FreeRTOS-Kernel 
    freertos_config
)
