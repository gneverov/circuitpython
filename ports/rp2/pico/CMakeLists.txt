cmake_minimum_required(VERSION 3.13)

add_library(pico_helper INTERFACE)

target_sources(pico_helper INTERFACE
    dma.c
    fifo.c
    gpio.c
    pio.c
    pwm.c
    terminal_boot.c
    terminal_uart.c
    uart.c
)

if(PICO_CYW43_SUPPORTED)
    target_sources(pico_helper INTERFACE
        cyw43_driver.c
        cyw43_lwip.c
    )

    target_compile_definitions(pico_helper INTERFACE
        CYW43_LWIP=1
    )    
endif()

target_include_directories(pico_helper INTERFACE include)

target_link_libraries(pico_helper INTERFACE
    FreeRTOS-Kernel 
    freertos_config
    hardware_uart
    pico_stdlib
)
