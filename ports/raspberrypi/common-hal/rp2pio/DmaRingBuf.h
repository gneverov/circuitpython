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

#include "py/mpprint.h"
#include "src/rp2_common/hardware_dma/include/hardware/dma.h"

typedef struct _rp2pio_dmaringbuf rp2pio_dmaringbuf_t;

typedef void (*rp2pio_dmaringbuf_handler_t)(rp2pio_dmaringbuf_t *ringbuf);

struct _rp2pio_dmaringbuf {
    uint channel;
    uint size;
    void *buffer;
    volatile uint next_read;
    volatile uint next_write;
    volatile uint trans_count;
    bool tx;
    uint max_transfer_count;
    enum dma_channel_transfer_size transfer_size;

    rp2pio_dmaringbuf_handler_t handler;

    uint int_count;
};

void common_hal_rp2pio_dmaringbuf_init(rp2pio_dmaringbuf_t *ringbuf, bool tx);

bool common_hal_rp2pio_dmaringbuf_alloc(rp2pio_dmaringbuf_t *ringbuf, uint ring_size_bits, uint dreq, uint max_transfer_count, enum dma_channel_transfer_size transfer_size, bool bswap, volatile void *target_addr);

void common_hal_rp2pio_dmaringbuf_deinit(rp2pio_dmaringbuf_t *ringbuf);

void common_hal_rp2pio_dmaringbuf_sync(rp2pio_dmaringbuf_t *ringbuf);

void common_hal_rp2pio_dmaringbuf_flush(rp2pio_dmaringbuf_t *ringbuf);

size_t common_hal_rp2pio_dmaringbuf_acquire(rp2pio_dmaringbuf_t *ringbuf, void **buf);

void common_hal_rp2pio_dmaringbuf_release(rp2pio_dmaringbuf_t *ringbuf, size_t bufsize);

size_t common_hal_rp2pio_dmaringbuf_transfer(rp2pio_dmaringbuf_t *ringbuf, void *buf, size_t bufsize);

void common_hal_rp2pio_dmaringbuf_clear(rp2pio_dmaringbuf_t *ringbuf);

void common_hal_rp2pio_dmaringbuf_set_enabled(rp2pio_dmaringbuf_t *ringbuf, bool enable);

void common_hal_rp2pio_dmaringbuf_set_handler(rp2pio_dmaringbuf_t *ringbuf, rp2pio_dmaringbuf_handler_t handler);

void common_hal_rp2pio_dmaringbuf_debug(const mp_print_t *print, rp2pio_dmaringbuf_t *ringbuf);
