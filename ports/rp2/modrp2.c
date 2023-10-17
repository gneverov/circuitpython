/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <malloc.h>
#include <memory.h>

#include "py/mperrno.h"
#include "py/runtime.h"
#include "drivers/dht/dht.h"
#include "modrp2.h"
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"

#if MICROPY_PY_NETWORK_CYW43
#ifndef MICROPY_PY_NETWORK_HOSTNAME_MAX_LEN
#define MICROPY_PY_NETWORK_HOSTNAME_MAX_LEN (16)
#endif

char mod_network_country_code[2] = "XX";

#ifndef MICROPY_PY_NETWORK_HOSTNAME_DEFAULT
#error "MICROPY_PY_NETWORK_HOSTNAME_DEFAULT must be set in mpconfigport.h or mpconfigboard.h"
#endif

char mod_network_hostname[MICROPY_PY_NETWORK_HOSTNAME_MAX_LEN] = MICROPY_PY_NETWORK_HOSTNAME_DEFAULT;
#endif

// Improved version of
// https://github.com/raspberrypi/pico-examples/blob/master/picoboard/button/button.c
STATIC bool __no_inline_not_in_flash_func(bootsel_button)(void) {
    const uint CS_PIN_INDEX = 1;

    // Disable interrupts and the other core since they might be
    // executing code from flash and we are about to temporarily
    // disable flash access.
    mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();

    // Set the CS pin to high impedance.
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        (GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB),
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    // Delay without calling any functions in flash.
    uint32_t start = timer_hw->timerawl;
    while ((uint32_t)(timer_hw->timerawl - start) <= MICROPY_HW_BOOTSEL_DELAY_US) {
        ;
    }

    // The HI GPIO registers in SIO can observe and control the 6 QSPI pins.
    // The button pulls the QSPI_SS pin *low* when pressed.
    bool button_state = !(sio_hw->gpio_hi_in & (1 << CS_PIN_INDEX));

    // Restore the QSPI_SS pin so we can use flash again.
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
        (GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB),
        IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);

    MICROPY_END_ATOMIC_SECTION(atomic_state);

    return button_state;
}

STATIC mp_obj_t rp2_bootsel_button(void) {
    return MP_OBJ_NEW_SMALL_INT(bootsel_button());
}
MP_DEFINE_CONST_FUN_OBJ_0(rp2_bootsel_button_obj, rp2_bootsel_button);

mp_obj_t __not_in_flash_func(rp2_heap_size)(size_t n_args, const mp_obj_t *args) {
    const extern size_t mp_gc_heap_size;
    assert(((uintptr_t)&mp_gc_heap_size >= XIP_BASE) && ((uintptr_t)&mp_gc_heap_size < SRAM_BASE));

    if (n_args > 0) {
        mp_int_t new_size = mp_obj_get_int(args[0]);
        uint8_t *buffer = malloc(FLASH_SECTOR_SIZE);
        if (!buffer) {
            mp_raise_OSError(MP_ENOMEM);
        }
        uintptr_t flash_offset = ((uintptr_t)&mp_gc_heap_size) & ~(FLASH_SECTOR_SIZE - 1);
        memcpy(buffer, (void *)flash_offset, FLASH_SECTOR_SIZE);
        *(size_t *)&buffer[ ((uintptr_t)&mp_gc_heap_size) & (FLASH_SECTOR_SIZE - 1)] = new_size;

        assert(flash_offset != ((uintptr_t)rp2_heap_size & ~(FLASH_SECTOR_SIZE - 1)));
        mp_uint_t state = MICROPY_BEGIN_ATOMIC_SECTION();
        flash_range_erase(flash_offset - XIP_BASE, FLASH_SECTOR_SIZE);
        flash_range_program(flash_offset - XIP_BASE, buffer, FLASH_SECTOR_SIZE);
        MICROPY_END_ATOMIC_SECTION(state);
        return mp_const_none;
    } else {
        return MP_OBJ_NEW_SMALL_INT(mp_gc_heap_size);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rp2_heap_size_obj, 0, 1, rp2_heap_size);

STATIC const mp_rom_map_elem_t rp2_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),            MP_ROM_QSTR(MP_QSTR_rp2) },
    { MP_ROM_QSTR(MP_QSTR_Flash),               MP_ROM_PTR(&rp2_flash_type) },
    { MP_ROM_QSTR(MP_QSTR_bootsel_button),      MP_ROM_PTR(&rp2_bootsel_button_obj) },
    { MP_ROM_QSTR(MP_QSTR_heap_size),           MP_ROM_PTR(&rp2_heap_size_obj) },

    MICROPY_PORT_NETWORK_INTERFACES
};
STATIC MP_DEFINE_CONST_DICT(rp2_module_globals, rp2_module_globals_table);

const mp_obj_module_t mp_module_rp2 = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&rp2_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__rp2, mp_module_rp2);
