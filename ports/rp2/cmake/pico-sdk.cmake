set(PICO_CLIB picolibc)
set(PICO_STDIO_UART OFF)
pico_sdk_init()

# Break non-essential dependency from pico_stdlib to pico_stdio.
# We do not want to use pico_stdio.
target_remove_property_value(pico_stdlib INTERFACE_LINK_LIBRARIES pico_stdio)
target_remove_property_value(pico_stdlib_headers INTERFACE_LINK_LIBRARIES pico_stdio_headers)
target_include_directories(pico_stdlib_headers INTERFACE ${PICO_SDK_PATH}/src/rp2_common/pico_stdio/include)

# Break non-essential dependency from pico_runtime to pico_printf and pico_malloc.
# We do not want to use pico_printf or pico_malloc.
target_remove_property_value(pico_runtime INTERFACE_LINK_LIBRARIES pico_printf pico_malloc)
target_remove_property_value(pico_runtime_headers INTERFACE_LINK_LIBRARIES pico_printf_headers pico_malloc_headers)

# Remove GCC specs option. We do no use specs with Picolibc.
target_remove_property_value(pico_runtime INTERFACE_LINK_OPTIONS --specs=nosys.specs)

# We use the the standard FreeRTOS mode of TinyUSB, not the specialized Pico mode.
target_remove_property_value(tinyusb_common_base INTERFACE_COMPILE_DEFINITIONS CFG_TUSB_OS=OPT_OS_PICO)
target_compile_definitions(tinyusb_common_base INTERFACE CFG_TUSB_OS=OPT_OS_FREERTOS)
