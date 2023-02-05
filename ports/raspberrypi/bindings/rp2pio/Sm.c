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

#include "bindings/rp2pio/Sm.h"
#include "bindings/rp2pio/Loop.h"
#include "common-hal/rp2pio/Dma.h"
#include "common-hal/rp2pio/DmaRingBuf.h"
#include "common-hal/rp2pio/Loop.h"
#include "common-hal/rp2pio/Pio.h"
#include "common-hal/rp2pio/Sm.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "src/rp2_common/hardware_pio/include/hardware/pio.h"

STATIC mp_obj_t rp2pio_sm_wait_handler(mp_obj_t self_obj, mp_obj_t tx_obj, mp_obj_t exc_obj);

STATIC mp_obj_t rp2pio_sm_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum {
        ARG_program,
        ARG_pins,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_program, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_pins, MP_ARG_REQUIRED | MP_ARG_OBJ },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_buffer_info_t program_buf;
    mp_get_buffer_raise(args[ARG_program].u_obj, &program_buf, MP_BUFFER_READ);
    pio_program_t program = { program_buf.buf, program_buf.len / sizeof(uint16_t), -1 };

    mp_arg_validate_type(args[ARG_pins].u_obj, &mp_type_list, MP_QSTR_pins);
    size_t num_pins;
    mp_obj_t *pins;
    mp_obj_list_get(args[ARG_pins].u_obj, &num_pins, &pins);
    for (uint i = 0; i < num_pins; i++) {
        mp_arg_validate_type(pins[i], &mcu_pin_type, MP_QSTR_pins);
    }

    rp2pio_pioslice_obj_t *pio_slice = m_new_obj(rp2pio_pioslice_obj_t);
    if (!common_hal_rp2pio_pioslice_claim(pio_slice, NULL, &program, 1, num_pins, pins)) {
        mp_raise_OSError(errno);
    }
    uint sm = __builtin_ctz(pio_slice->sm_mask);

    rp2pio_sm_obj_t *self = m_new_obj(rp2pio_sm_obj_t);
    if (!common_hal_rp2pio_sm_init(self, type, pio_slice, sm)) {
        common_hal_rp2pio_sm_deinit(self);
        mp_raise_OSError(errno);
    }

    return MP_OBJ_FROM_PTR(self);
}

STATIC void _abort_waiters(mp_obj_t self_obj) {
    mp_obj_t exc_obj = mp_obj_new_exception_msg(&mp_type_RuntimeError, NULL);
    rp2pio_sm_wait_handler(self_obj, mp_obj_new_bool(false), exc_obj);
    rp2pio_sm_wait_handler(self_obj, mp_obj_new_bool(true), exc_obj);
}

STATIC mp_obj_t rp2pio_sm_deinit(mp_obj_t self_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    common_hal_rp2pio_sm_deinit(self);
    _abort_waiters(self_obj);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(rp2pio_sm_deinit_obj, rp2pio_sm_deinit);

STATIC mp_obj_t rp2pio_sm_set_pins(size_t n_args, const mp_obj_t *all_args) {
    enum {
        ARG_self,
        ARG_pin_type,
        ARG_pin_base,
        ARG_pin_count,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_pin_type, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_pin_base, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_pin_count, MP_ARG_REQUIRED | MP_ARG_INT },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, 0, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(args[ARG_self].u_obj);
    if (!common_hal_rp2pio_sm_set_pins(self, args[ARG_pin_type].u_int, args[ARG_pin_base].u_int, args[ARG_pin_count].u_int)) {
        mp_raise_RuntimeError(NULL);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rp2pio_sm_set_pins_obj, 4, 4, rp2pio_sm_set_pins);

STATIC mp_obj_t rp2pio_sm_set_pulls(size_t n_args, const mp_obj_t *all_args) {
    enum {
        ARG_self,
        ARG_pin_mask,
        ARG_pull_up,
        ARG_pull_down,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_pin_mask, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_pull_up, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_pull_down, MP_ARG_REQUIRED | MP_ARG_INT },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, 0, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(args[ARG_self].u_obj);
    if (!common_hal_rp2pio_sm_set_pulls(self, args[ARG_pin_mask].u_int, args[ARG_pull_up].u_int, args[ARG_pull_down].u_int)) {
        mp_raise_RuntimeError(NULL);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rp2pio_sm_set_pulls_obj, 4, 4, rp2pio_sm_set_pulls);

STATIC mp_obj_t rp2pio_sm_set_sideset(size_t n_args, const mp_obj_t *all_args) {
    enum {
        ARG_self,
        ARG_bit_count,
        ARG_optional,
        ARG_pindirs,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_bit_count, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_optional, MP_ARG_REQUIRED | MP_ARG_BOOL },
        { MP_QSTR_pindirs, MP_ARG_REQUIRED | MP_ARG_BOOL },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, 0, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(args[ARG_self].u_obj);
    sm_config_set_sideset(&self->config, args[ARG_bit_count].u_int, args[ARG_optional].u_bool, args[ARG_pindirs].u_bool);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rp2pio_sm_set_sideset_obj, 4, 4, rp2pio_sm_set_sideset);

STATIC mp_obj_t rp2pio_sm_set_frequency(mp_obj_t self_obj, mp_obj_t freq_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    mp_float_t freq = mp_obj_get_float(freq_obj);
    freq = common_hal_rp2pio_sm_set_frequency(self, freq);
    return mp_obj_new_float(freq);
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_set_frequency_obj, rp2pio_sm_set_frequency);

STATIC mp_obj_t rp2pio_sm_set_wrap(size_t n_args, const mp_obj_t *all_args) {
    enum {
        ARG_self,
        ARG_wrap_target,
        ARG_wrap,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_wrap_target, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_wrap, MP_ARG_REQUIRED | MP_ARG_INT },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, 0, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(args[ARG_self].u_obj);
    common_hal_rp2pio_sm_set_wrap(self, args[ARG_wrap_target].u_int, args[ARG_wrap].u_int);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rp2pio_sm_set_wrap_obj, 3, 3, rp2pio_sm_set_wrap);

STATIC mp_obj_t rp2pio_sm_set_shift(size_t n_args, const mp_obj_t *all_args) {
    enum {
        ARG_self,
        ARG_out,
        ARG_shift_right,
        ARG_auto,
        ARG_threshold,
    };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_self, MP_ARG_REQUIRED | MP_ARG_OBJ },
        { MP_QSTR_out, MP_ARG_REQUIRED | MP_ARG_BOOL },
        { MP_QSTR_shift_right, MP_ARG_REQUIRED | MP_ARG_BOOL },
        { MP_QSTR_auto, MP_ARG_REQUIRED | MP_ARG_BOOL },
        { MP_QSTR_threshold, MP_ARG_REQUIRED | MP_ARG_INT },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, 0, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(args[ARG_self].u_obj);
    common_hal_rp2pio_sm_set_shift(self, args[ARG_out].u_bool, args[ARG_shift_right].u_bool, args[ARG_auto].u_bool, args[ARG_threshold].u_int);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rp2pio_sm_set_shift_obj, 5, 5, rp2pio_sm_set_shift);

STATIC mp_obj_t rp2pio_sm_reset(mp_obj_t self_obj, mp_obj_t initial_pc_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    uint initial_pc = mp_obj_int_get_uint_checked(initial_pc_obj);

    common_hal_rp2pio_sm_reset(self, initial_pc);
    _abort_waiters(self_obj);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_reset_obj, rp2pio_sm_reset);

STATIC mp_obj_t rp2pio_sm_set_enabled(mp_obj_t self_obj, mp_obj_t enabled_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    pio_sm_set_enabled(self->pio_slice->pio, self->sm, mp_obj_is_true(enabled_obj));
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_set_enabled_obj, rp2pio_sm_set_enabled);

STATIC mp_obj_t rp2pio_sm_exec(mp_obj_t self_obj, mp_obj_t instr_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    uint instr = mp_obj_int_get_uint_checked(instr_obj);
    pio_sm_exec(self->pio_slice->pio, self->sm, instr);
    return pio_sm_is_exec_stalled(self->pio_slice->pio, self->sm) ? mp_const_false : mp_const_true;
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_exec_obj, rp2pio_sm_exec);

STATIC mp_obj_t rp2pio_sm_debug(mp_obj_t self_obj, mp_obj_t tx_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    mp_obj_t result = mp_obj_new_dict(3);
    mp_obj_t sm_dict = mp_obj_new_dict(8);
    mp_obj_dict_store(sm_dict, MP_OBJ_NEW_QSTR(MP_QSTR_sm), mp_obj_new_int_from_uint(self->sm));
    mp_obj_dict_store(sm_dict, MP_OBJ_NEW_QSTR(MP_QSTR_clkdiv), mp_obj_new_int_from_uint(self->config.clkdiv));
    mp_obj_dict_store(sm_dict, MP_OBJ_NEW_QSTR(MP_QSTR_execctrl), mp_obj_new_int_from_uint(self->config.execctrl));
    mp_obj_dict_store(sm_dict, MP_OBJ_NEW_QSTR(MP_QSTR_shiftctrl), mp_obj_new_int_from_uint(self->config.shiftctrl));
    mp_obj_dict_store(sm_dict, MP_OBJ_NEW_QSTR(MP_QSTR_pinctrl), mp_obj_new_int_from_uint(self->config.pinctrl));
    mp_obj_dict_store(sm_dict, MP_OBJ_NEW_QSTR(MP_QSTR_rx_level), mp_obj_new_int_from_uint(pio_sm_get_rx_fifo_level(self->pio_slice->pio, self->sm)));
    mp_obj_dict_store(sm_dict, MP_OBJ_NEW_QSTR(MP_QSTR_tx_level), mp_obj_new_int_from_uint(pio_sm_get_tx_fifo_level(self->pio_slice->pio, self->sm)));
    mp_obj_dict_store(result, MP_OBJ_NEW_QSTR(MP_QSTR_sm), sm_dict);

    mp_obj_t pio_dict = mp_obj_new_dict(8);
    mp_obj_dict_store(pio_dict, MP_OBJ_NEW_QSTR(MP_QSTR_pio), mp_obj_new_int_from_uint(pio_get_index(self->pio_slice->pio)));
    mp_obj_dict_store(pio_dict, MP_OBJ_NEW_QSTR(MP_QSTR_offset), mp_obj_new_int_from_uint(self->pio_slice->loaded_offset));
    mp_obj_dict_store(pio_dict, MP_OBJ_NEW_QSTR(MP_QSTR_sm_mask), mp_obj_new_int_from_uint(self->pio_slice->sm_mask));
    mp_obj_dict_store(pio_dict, MP_OBJ_NEW_QSTR(MP_QSTR_pin_mask), mp_obj_new_int_from_uint(self->pio_slice->pin_mask));
    mp_obj_dict_store(result, MP_OBJ_NEW_QSTR(MP_QSTR_pio), pio_dict);

    mp_obj_dict_store(result, MP_OBJ_NEW_QSTR(MP_QSTR_loop), self->loop_obj);
    mp_obj_dict_store(result, MP_OBJ_NEW_QSTR(MP_QSTR_rx_futures), self->rx_futures);
    mp_obj_dict_store(result, MP_OBJ_NEW_QSTR(MP_QSTR_tx_futures), self->tx_futures);

    bool tx = mp_obj_is_true(tx_obj);
    rp2pio_dmaringbuf_t *dma_ringbuf = tx ? &self->tx_ringbuf : &self->rx_ringbuf;
    common_hal_rp2pio_dmaringbuf_debug(&mp_plat_print, dma_ringbuf);

    common_hal_rp2pio_pio_debug(&mp_plat_print, self->pio_slice->pio);

    return result;
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_debug_obj, rp2pio_sm_debug);

STATIC mp_obj_t rp2pio_sm_recv(mp_obj_t self_obj, mp_obj_t bufsize_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);

    if (!mp_obj_is_small_int(bufsize_obj)) {
        mp_raise_ValueError(NULL);
    }
    size_t bufsize = MP_OBJ_SMALL_INT_VALUE(bufsize_obj);

    bufsize = common_hal_rp2pio_dmaringbuf_transfer(&self->rx_ringbuf, NULL, bufsize);
    if (!bufsize) {
        return mp_const_empty_bytes;
    }

    mp_obj_str_t *bytes = m_new_obj(mp_obj_str_t);
    bytes->base.type = &mp_type_bytes;
    byte *buffer = m_new(byte, bufsize);
    if (!buffer) {
        mp_raise_RuntimeError(NULL);
    }
    bytes->len = common_hal_rp2pio_dmaringbuf_transfer(&self->rx_ringbuf, buffer, bufsize);
    bytes->data = buffer;
    return MP_OBJ_FROM_PTR(bytes);
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_recv_obj, rp2pio_sm_recv);

STATIC mp_obj_t rp2pio_sm_recvinto(mp_obj_t self_obj, mp_obj_t buffer_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);

    mp_buffer_info_t buffer;
    mp_get_buffer_raise(buffer_obj, &buffer, MP_BUFFER_WRITE);

    size_t result = common_hal_rp2pio_dmaringbuf_transfer(&self->rx_ringbuf, buffer.buf, buffer.len);
    return MP_OBJ_NEW_SMALL_INT(result);
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_recvinto_obj, rp2pio_sm_recvinto);

STATIC mp_obj_t rp2pio_sm_send(mp_obj_t self_obj, mp_obj_t buffer_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);

    mp_buffer_info_t buffer;
    mp_get_buffer_raise(buffer_obj, &buffer, MP_BUFFER_READ);

    size_t result = common_hal_rp2pio_dmaringbuf_transfer(&self->tx_ringbuf, buffer.buf, buffer.len);
    return MP_OBJ_NEW_SMALL_INT(result);
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_send_obj, rp2pio_sm_send);

STATIC void _irq_handler(PIO pio, enum pio_interrupt_source source, void *context) {
    rp2pio_loop_call_soon_entry_t *entry = context;

    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(entry->args[2]);
    bool tx = common_hal_rp2pio_sm_tx_from_source(source, self->sm);
    common_hal_rp2pio_sm_end_wait(self, tx);

    common_hal_rp2pio_loop_call_soon_isrsafe(entry);
}

STATIC mp_obj_t rp2pio_sm_wait_handler(mp_obj_t self_obj, mp_obj_t tx_obj, mp_obj_t exc_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    bool tx = mp_obj_is_true(tx_obj);

    mp_obj_t list_obj = tx ? self->tx_futures : self->rx_futures;
    mp_obj_iter_buf_t iter_buf;
    mp_obj_t iter_obj = mp_getiter(list_obj, &iter_buf);
    mp_obj_t future_obj = mp_iternext(iter_obj);
    while (future_obj != MP_OBJ_STOP_ITERATION) {
        mp_obj_t args[3];
        mp_load_method(future_obj, exc_obj == mp_const_none ? MP_QSTR_set_result : MP_QSTR_set_exception, args);
        args[2] = exc_obj;
        mp_call_method_n_kw(1, 0, args);
        future_obj = mp_iternext(iter_obj);
    }
    return mp_obj_list_clear(list_obj);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(rp2pio_sm_wait_handler_obj, rp2pio_sm_wait_handler);

STATIC mp_obj_t rp2pio_sm_wait(mp_obj_t self_obj, mp_obj_t tx_obj) {
    rp2pio_sm_obj_t *self = MP_OBJ_TO_PTR(self_obj);

    if (!mp_obj_is_bool(tx_obj)) {
        mp_raise_ValueError(NULL);
    }
    bool tx = mp_obj_is_true(tx_obj);

    mp_obj_t dest[2];
    mp_load_method(self->loop_obj, MP_QSTR_create_future, dest);
    mp_obj_t future_obj = mp_call_function_1(dest[0], dest[1]);

    mp_obj_t list_obj = tx ? self->tx_futures : self->rx_futures;
    mp_obj_list_append(list_obj, future_obj);

    rp2pio_loop_obj_t *native_loop = rp2pio_get_native_loop(self->loop_obj);
    mp_obj_t args[] = { self_obj, tx_obj, mp_const_none };
    void *context = common_hal_rp2pio_loop_call_soon_entry_alloc(native_loop, self->loop_obj, MP_OBJ_FROM_PTR(&rp2pio_sm_wait_handler_obj), 3, args);
    if (!common_hal_rp2pio_sm_begin_wait(self, tx, _irq_handler, context)) {
        rp2pio_sm_wait_handler(self_obj, tx_obj, mp_const_none);
    }
    return future_obj;
}
MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_sm_wait_obj, rp2pio_sm_wait);

STATIC const mp_rom_map_elem_t rp2pio_sm_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&rp2pio_sm_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pins), MP_ROM_PTR(&rp2pio_sm_set_pins_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pulls), MP_ROM_PTR(&rp2pio_sm_set_pulls_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_sideset), MP_ROM_PTR(&rp2pio_sm_set_sideset_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_frequency), MP_ROM_PTR(&rp2pio_sm_set_frequency_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_wrap), MP_ROM_PTR(&rp2pio_sm_set_wrap_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_shift), MP_ROM_PTR(&rp2pio_sm_set_shift_obj) },

    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&rp2pio_sm_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_enabled), MP_ROM_PTR(&rp2pio_sm_set_enabled_obj) },
    { MP_ROM_QSTR(MP_QSTR_exec), MP_ROM_PTR(&rp2pio_sm_exec_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&rp2pio_sm_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvinto), MP_ROM_PTR(&rp2pio_sm_recvinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&rp2pio_sm_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_wait), MP_ROM_PTR(&rp2pio_sm_wait_obj) },

    { MP_ROM_QSTR(MP_QSTR_debug), MP_ROM_PTR(&rp2pio_sm_debug_obj) },
};
STATIC MP_DEFINE_CONST_DICT(rp2pio_sm_locals_dict, rp2pio_sm_locals_dict_table);

const mp_obj_type_t rp2pio_sm_type = {
    { &mp_type_type },
    .name = MP_QSTR_Sm,
    .make_new = rp2pio_sm_make_new,
    .locals_dict = (mp_obj_dict_t *)&rp2pio_sm_locals_dict,
};
