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

#pragma once

#include "py/obj.h"

typedef struct _rp2pio_loop_call_soon_entry_t rp2pio_loop_call_soon_entry_t;

typedef struct {
    mp_obj_base_t base;
    rp2pio_loop_call_soon_entry_t *call_soon_list_head;
    rp2pio_loop_call_soon_entry_t **call_soon_list_tail;
} rp2pio_loop_obj_t;

struct _rp2pio_loop_call_soon_entry_t {
    rp2pio_loop_call_soon_entry_t *next;
    rp2pio_loop_obj_t *native_loop;
    size_t n_args;
    mp_obj_t *args;
};

extern mp_obj_t common_hal_rp2pio_event_loop_obj;

void common_hal_rp2pio_loop_init(rp2pio_loop_obj_t *native_loop, const mp_obj_type_t *type);

rp2pio_loop_call_soon_entry_t *common_hal_rp2pio_loop_call_soon_entry_alloc(rp2pio_loop_obj_t *native_loop, mp_obj_t loop_obj,  mp_obj_t fun_obj, size_t n_args, mp_obj_t *args);

void common_hal_rp2pio_loop_call_soon_entry_free(rp2pio_loop_call_soon_entry_t *entry);

void common_hal_rp2pio_loop_call_soon_isrsafe(rp2pio_loop_call_soon_entry_t *entry);

void common_hal_rp2pio_loop_poll_isr(rp2pio_loop_obj_t *native_loop, mp_obj_t loop_obj);
