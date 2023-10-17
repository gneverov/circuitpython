# cmake file for Raspberry Pi Pico W

set(PICO_BOARD "pico_w")
set(PICO_CYW43_SUPPORTED ON)

set(MICROPY_PY_LWIP ON)
set(MICROPY_PY_NETWORK_CYW43 ON)

# Bluetooth
set(MICROPY_PY_BLUETOOTH OFF)
set(MICROPY_BLUETOOTH_BTSTACK OFF)
set(MICROPY_PY_BLUETOOTH_CYW43 OFF)

# Board specific version of the frozen manifest
set(MICROPY_FROZEN_MANIFEST ${MICROPY_BOARD_DIR}/manifest.py)
