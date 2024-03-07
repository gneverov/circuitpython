cmake_minimum_required(VERSION 3.13)

add_library(newlib_helper INTERFACE)

target_sources(newlib_helper INTERFACE
    devfs.c
    flash.c
    # freertos.c
    lock.c
    newlib.c
    posix.c
    random.c
    vfs.c
)

target_include_directories(newlib_helper INTERFACE include)

target_link_libraries(newlib_helper INTERFACE
    FreeRTOS-Kernel 
    freertos_config
    freertos_helper
    pico_flash
)


add_library(newlib_fatfs INTERFACE)
target_sources(newlib_fatfs INTERFACE fatfs.c)
target_compile_definitions(newlib_fatfs INTERFACE NEWLIB_FATFS)
target_link_libraries(newlib_fatfs INTERFACE
    fatfs
    newlib_helper
)


add_library(newlib_dhara INTERFACE)
target_sources(newlib_dhara INTERFACE dhara.c)
target_compile_definitions(newlib_dhara INTERFACE NEWLIB_DHARA)
target_link_libraries(newlib_dhara INTERFACE
    dhara
    newlib_helper
)
