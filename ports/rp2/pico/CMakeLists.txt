cmake_minimum_required(VERSION 3.13)

add_library(pico_helper INTERFACE)

target_sources(pico_helper INTERFACE
    dma.c
    fifo.c
    gpio.c
    pio.c
    pwm.c
    sdcard.c
    spi.c
    terminal.c
    terminal_boot.c
    terminal_uart.c
    time.c
    uart.c
)

target_include_directories(pico_helper INTERFACE include)

target_link_libraries(pico_helper INTERFACE
    FreeRTOS-Kernel 
    hardware_uart
    pico_aon_timer
)

target_link_options(pico_helper INTERFACE
    LINKER:--wrap=settimeofday
)

if(PICO_CYW43_SUPPORTED)
    add_library(cyw43_helper INTERFACE)

    target_sources(cyw43_helper INTERFACE
        cyw43_driver.c
        cyw43_lwip.c
    )

    target_compile_definitions(cyw43_helper INTERFACE
        CYW43_LWIP=1
    )

    target_include_directories(cyw43_helper INTERFACE include)
endif()
