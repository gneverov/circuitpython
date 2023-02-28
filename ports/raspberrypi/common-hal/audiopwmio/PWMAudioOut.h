/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Jeff Epler for Adafruit Industries
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

#include "common-hal/rp2pio/DmaRingBuf.h"
#include "peripherals/pins.h"

typedef struct {
    mp_obj_base_t base;
    const mcu_pin_obj_t *a_channel;
    const mcu_pin_obj_t *b_channel;
    uint pwm_slice;
    rp2pio_dmaringbuf_t ringbuf;
    uint dma_timer;

    uint channel_count;
    uint input_bytes;
    uint output_bits;
    uint int_count;
} audiopwmio_pwmaudioout_obj_t;

void common_hal_audiopwmio_pwmaudioout_init(audiopwmio_pwmaudioout_obj_t *self, const mp_obj_type_t *type);

void common_hal_audiopwmio_pwmaudioout_construct(audiopwmio_pwmaudioout_obj_t *self, const mcu_pin_obj_t *a_channel, const mcu_pin_obj_t *b_channel, uint ring_size_bits, uint max_transfer_count, uint channel_count, uint sample_rate, uint input_bytes, uint output_bits, bool phase_correct);

bool common_hal_audiopwmio_pwmaudioout_deinited(audiopwmio_pwmaudioout_obj_t *self);

void common_hal_audiopwmio_pwmaudioout_deinit(audiopwmio_pwmaudioout_obj_t *self);

mp_uint_t common_hal_audiopwmio_pwmaudioout_write(mp_obj_t self_obj, const void *buf, mp_uint_t size, int *errcode);

mp_uint_t common_hal_audiopwmio_pwmaudioout_ioctl(mp_obj_t self_obj, mp_uint_t request, uintptr_t arg, int *errcode);

size_t common_hal_audiopwmio_pwmaudioout_play(audiopwmio_pwmaudioout_obj_t *self, const void *buf, size_t len);

void common_hal_audiopwmio_pwmaudioout_stop(audiopwmio_pwmaudioout_obj_t *self);

bool common_hal_audiopwmio_pwmaudioout_get_playing(audiopwmio_pwmaudioout_obj_t *self);

uint common_hal_audiopwmio_pwmaudioout_get_stalled(audiopwmio_pwmaudioout_obj_t *self);

uint common_hal_audiopwmio_pwmaudioout_get_available(audiopwmio_pwmaudioout_obj_t *self);

#ifndef NDEBUG
void common_hal_audiopwmio_pwmaudioout_debug(const mp_print_t *print, audiopwmio_pwmaudioout_obj_t *self);
#endif
