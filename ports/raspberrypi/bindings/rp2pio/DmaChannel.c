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

#include "bindings/rp2pio/DmaChannel.h"
#include "bindings/rp2pio/Loop.h"
#include "common-hal/rp2pio/Dma.h"
#include "common-hal/rp2pio/Loop.h"
#include "py/mperrno.h"
#include "py/runtime.h"
#include "src/rp2_common/hardware_dma/include/hardware/dma.h"

STATIC void _irq_handler(uint channel, mp_obj_t context_obj);

STATIC mp_obj_t _loop_callback(mp_obj_t channel_obj, mp_obj_t future_obj) {
    int channel = MP_OBJ_SMALL_INT_VALUE(channel_obj);
    dma_channel_acknowledge_irq1(channel);
    if (dma_channel_is_busy(channel)) {
        // reset interrupt
        // return mp_const_none;
        mp_raise_RuntimeError(NULL);
    }

    dma_channel_unclaim(channel);
    mp_obj_t args[3];
    mp_load_method(future_obj, MP_QSTR_set_result, args);
    args[2] = mp_const_none;
    return mp_call_method_n_kw(1, 0, args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(_loop_callback_obj, _loop_callback);

STATIC void _irq_handler(uint channel, void *context) {
    rp2pio_loop_call_soon_entry_t *entry = context;
    common_hal_rp2pio_loop_call_soon_isrsafe(entry);
}

STATIC mp_obj_t rp2pio_dmachannel_transfer(mp_obj_t src_obj, mp_obj_t dst_obj) {
    mp_buffer_info_t src_buf;
    mp_get_buffer_raise(src_obj, &src_buf, MP_BUFFER_READ);

    mp_buffer_info_t dst_buf;
    mp_get_buffer_raise(dst_obj, &dst_buf, MP_BUFFER_WRITE);

    if (dst_buf.len < src_buf.len) {
        mp_raise_IndexError(NULL);
    }

    mp_obj_t loop_obj = common_hal_rp2pio_event_loop_obj;
    if (!mp_obj_is_obj(loop_obj)) {
        mp_raise_RuntimeError(NULL);
    }

    mp_obj_t dest[2];
    mp_load_method(loop_obj, MP_QSTR_create_future, dest);
    mp_obj_t future_obj = mp_call_function_1(dest[0], dest[1]);

    int channel = dma_claim_unused_channel(false);
    if (channel == -1) {
        mp_raise_OSError(MP_EBUSY);
    }

    rp2pio_loop_obj_t *native_loop = rp2pio_get_native_loop(loop_obj);
    mp_obj_t args[] = {MP_OBJ_NEW_SMALL_INT(channel), future_obj };
    void *context = common_hal_rp2pio_loop_call_soon_entry_alloc(native_loop, loop_obj, MP_OBJ_FROM_PTR(&_loop_callback_obj), 2, args);
    common_hal_rp2pio_dma_set_irq(channel, _irq_handler, context);

    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(channel, &c, dst_buf.buf, src_buf.buf, src_buf.len, true);

    return future_obj;
}

MP_DEFINE_CONST_FUN_OBJ_2(rp2pio_dmachannel_transfer_obj, rp2pio_dmachannel_transfer);

STATIC mp_obj_t rp2pio_dmachannel_stop(void) {
    return mp_const_none;
}

MP_DEFINE_CONST_FUN_OBJ_0(rp2pio_dmachannel_stop_obj, rp2pio_dmachannel_stop);
