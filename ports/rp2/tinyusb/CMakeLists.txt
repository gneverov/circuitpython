cmake_minimum_required(VERSION 3.13)

add_library(tinyusb_helper INTERFACE)

target_sources(tinyusb_helper INTERFACE
    cdc_device_cb.c
    msc_device.c
    terminal_usb.c
    tusb_config.c
    tusb_lock.c
    usbd_cb.c
)

if (MICROPY_PY_LWIP)
    target_sources(tinyusb_helper INTERFACE net_device_lwip.c)
endif()

target_include_directories(tinyusb_helper INTERFACE include)

target_link_libraries(tinyusb_helper INTERFACE
    tinyusb_common
    tinyusb_device
)
