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

#include "cyw43.h"
#include "pico/cyw43_driver.h"

#include "py/obj.h"

extern const mp_obj_type_t mp_network_cyw43_type;

static mp_obj_t network_cyw43_init(void) {
    cyw43_driver_init();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(network_cyw43_init_obj, network_cyw43_init);

static mp_obj_t network_cyw43_deinit(void) {
    cyw43_driver_deinit();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(network_cyw43_deinit_obj, network_cyw43_deinit);

static const mp_rom_map_elem_t network_cyw43_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_cyw43) },
    { MP_ROM_QSTR(MP_QSTR___init__),        MP_ROM_PTR(&network_cyw43_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_init),             MP_ROM_PTR(&network_cyw43_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit),           MP_ROM_PTR(&network_cyw43_deinit_obj) },

    { MP_ROM_QSTR(MP_QSTR_WLAN),            MP_ROM_PTR(&mp_network_cyw43_type) }, \
    { MP_ROM_QSTR(MP_QSTR_STAT_IDLE),       MP_ROM_INT(CYW43_LINK_DOWN) }, \
    { MP_ROM_QSTR(MP_QSTR_STAT_CONNECTING), MP_ROM_INT(CYW43_LINK_JOIN) }, \
    { MP_ROM_QSTR(MP_QSTR_STAT_WRONG_PASSWORD), MP_ROM_INT(CYW43_LINK_BADAUTH) }, \
    { MP_ROM_QSTR(MP_QSTR_STAT_NO_AP_FOUND), MP_ROM_INT(CYW43_LINK_NONET) }, \
    { MP_ROM_QSTR(MP_QSTR_STAT_CONNECT_FAIL), MP_ROM_INT(CYW43_LINK_FAIL) }, \
    { MP_ROM_QSTR(MP_QSTR_STAT_GOT_IP),     MP_ROM_INT(CYW43_LINK_UP) },
};
static MP_DEFINE_CONST_DICT(network_cyw43_module_globals, network_cyw43_module_globals_table);

const mp_obj_module_t network_cyw43_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&network_cyw43_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_cyw43, network_cyw43_module);
MP_REGISTER_OBJECT(network_cyw43_module);
