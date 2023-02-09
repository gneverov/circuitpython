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
#include "src/rp2_common/hardware_pio/include/hardware/pio.h"

typedef struct {
    mp_obj_base_t base;
    PIO pio;
    pio_program_t program;
    uint loaded_offset;
    uint sm_mask;
    uint pin_mask;

} rp2pio_pioslice_obj_t;

void common_hal_rp2pio_pioslice_reset(void);

void common_hal_rp2pio_pioslice_init(rp2pio_pioslice_obj_t *self, const mp_obj_type_t *type, PIO pio, const pio_program_t *program);

void common_hal_rp2pio_pioslice_deinit(rp2pio_pioslice_obj_t *self);

bool common_hal_rp2pio_pioslice_claim(rp2pio_pioslice_obj_t *self, const mp_obj_type_t *type, const pio_program_t *program, uint num_sms, size_t num_pins, const mp_obj_t *pins);

void common_hal_rp2pio_pioslice_release_sm(rp2pio_pioslice_obj_t *self, uint sm);

void common_hal_rp2pio_pioslice_release_pin(rp2pio_pioslice_obj_t *self, uint pin);
