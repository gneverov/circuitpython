cmake_minimum_required(VERSION 3.13)

add_library(newlib_helper INTERFACE)

target_sources(newlib_helper INTERFACE
    lock.c
    newlib.c
    # terminal.c
)

target_include_directories(newlib_helper INTERFACE include)

target_link_libraries(newlib_helper INTERFACE
    FreeRTOS-Kernel 
    freertos_config
    freertos_helper
)
