cmake_minimum_required(VERSION 3.13)

add_library(tinyusb_helper INTERFACE)

target_sources(tinyusb_helper INTERFACE
    cdc_device_cb.c
    net_device_lwip.c
    terminal_usb.c
    tusb_config.c
    tusb_lock.c
    usbd_cb.c
)

target_include_directories(tinyusb_helper INTERFACE include)

target_link_libraries(tinyusb_helper INTERFACE
    # tinyusb
)
