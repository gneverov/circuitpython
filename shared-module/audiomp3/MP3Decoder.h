/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Scott Shawcroft
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

#include "py/obj.h"
#include "lib/mp3/src/mp3common.h"


typedef struct {
    mp_obj_base_t base;
    mp_obj_t stream_obj;
    MP3DecInfo *decoder;
    MP3FrameInfo frame_info;

    unsigned char *in_buffer;
    int in_buffer_size;
    int in_buffer_index;
} audiomp3_mp3file_obj_t;

void common_hal_audiomp3_mp3file_init(audiomp3_mp3file_obj_t *self, const mp_obj_type_t *type);

bool common_hal_audiomp3_mp3file_open(audiomp3_mp3file_obj_t *self, mp_obj_t stream_obj, int *errcode);

mp_uint_t common_hal_audiomp3_mp3file_read(mp_obj_t self_obj, void *buf, mp_uint_t size, int *errcode);

mp_uint_t common_hal_audiomp3_mp3file_ioctl(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode);

void common_hal_audiomp3_mp3file_deinit(audiomp3_mp3file_obj_t *self);

bool common_hal_audiomp3_mp3file_deinited(audiomp3_mp3file_obj_t *self);

uint32_t common_hal_audiomp3_mp3file_get_sample_rate(audiomp3_mp3file_obj_t *self);

uint8_t common_hal_audiomp3_mp3file_get_bits_per_sample(audiomp3_mp3file_obj_t *self);

uint8_t common_hal_audiomp3_mp3file_get_channel_count(audiomp3_mp3file_obj_t *self);
