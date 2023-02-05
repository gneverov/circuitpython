/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
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

#include "bindings/rp2pio/Loop.h"
#include "common-hal/rp2pio/Loop.h"
#include "py/runtime.h"

rp2pio_loop_obj_t *rp2pio_get_native_loop(mp_obj_t loop_obj) {
    rp2pio_loop_obj_t *native_loop = MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(loop_obj, &rp2pio_loop_type));
    if (!native_loop) {
        mp_raise_TypeError(NULL);
    }
    return native_loop;
}

STATIC mp_obj_t rp2pio_loop_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    rp2pio_loop_obj_t *self = m_new_obj(rp2pio_loop_obj_t);
    common_hal_rp2pio_loop_init(self, type);
    if ((n_args != 0) || (n_kw != 0)) {
        mp_raise_ValueError(NULL);
    }
    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t rp2pio_loop_poll_isr(mp_obj_t self_obj) {
    rp2pio_loop_obj_t *native_loop = rp2pio_get_native_loop(self_obj);
    common_hal_rp2pio_loop_poll_isr(native_loop, self_obj);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(rp2pio_loop_poll_isr_obj, rp2pio_loop_poll_isr);

STATIC const mp_rom_map_elem_t rp2pio_loop_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_poll_isr), MP_ROM_PTR(&rp2pio_loop_poll_isr_obj) },
};
STATIC MP_DEFINE_CONST_DICT(rp2pio_loop_locals_dict, rp2pio_loop_locals_dict_table);

const mp_obj_type_t rp2pio_loop_type = {
    { &mp_type_type },
    .name = MP_QSTR_Loop,
    .make_new = rp2pio_loop_make_new,
    .locals_dict = (mp_obj_dict_t *)&rp2pio_loop_locals_dict,
};

STATIC mp_obj_t rp2pio_set_event_loop(mp_obj_t loop_obj) {
    rp2pio_get_native_loop(loop_obj);
    common_hal_rp2pio_event_loop_obj = loop_obj;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(rp2pio_set_event_loop_obj, rp2pio_set_event_loop);

STATIC mp_obj_t rp2pio_get_event_loop(void) {
    return common_hal_rp2pio_event_loop_obj;
}
MP_DEFINE_CONST_FUN_OBJ_0(rp2pio_get_event_loop_obj, rp2pio_get_event_loop);
