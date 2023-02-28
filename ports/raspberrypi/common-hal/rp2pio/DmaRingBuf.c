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

#include "common-hal/rp2pio/DmaRingBuf.h"
#include "common-hal/rp2pio/Dma.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/runtime.h"

STATIC void _irq_handler(uint channel, void *context);

void common_hal_rp2pio_dmaringbuf_init(rp2pio_dmaringbuf_t *ringbuf, bool tx) {
    ringbuf->channel = -1u;
    ringbuf->size = 0;
    ringbuf->buffer = NULL;
    ringbuf->next_read = 0;
    ringbuf->next_write = 0;
    ringbuf->trans_count = 0;
    ringbuf->tx = tx;
    ringbuf->max_transfer_count = 0;
    ringbuf->transfer_size = DMA_SIZE_8;
    ringbuf->int_count = 0;
}

bool common_hal_rp2pio_dmaringbuf_alloc(rp2pio_dmaringbuf_t *ringbuf, uint ring_size_bits, uint dreq, uint max_transfer_count, enum dma_channel_transfer_size transfer_size, bool bswap, volatile void *target_addr) {
    assert(ring_size_bits >= 4);

    if (!peripherals_dma_channel_claim(&ringbuf->channel)) {
        errno = MP_EBUSY;
        return false;
    }

    void *buffer = common_hal_rp2pio_dma_alloc_aligned(ring_size_bits, false);
    if (!buffer) {
        errno = MP_ENOMEM;
        return false;
    }

    ringbuf->size = 1u << ring_size_bits;
    ringbuf->buffer = buffer;
    ringbuf->max_transfer_count = max_transfer_count ? max_transfer_count : ringbuf->size >> 1;
    ringbuf->transfer_size = transfer_size;

    uint channel = ringbuf->channel;
    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_read_increment(&c, ringbuf->tx);
    channel_config_set_write_increment(&c, !ringbuf->tx);
    channel_config_set_dreq(&c, dreq);
    channel_config_set_transfer_data_size(&c, transfer_size);
    channel_config_set_ring(&c, !ringbuf->tx, ring_size_bits);
    channel_config_set_bswap(&c, bswap);
    dma_channel_set_config(channel, &c, false);

    dma_channel_set_trans_count(channel, 0, false);
    if (ringbuf->tx) {
        dma_channel_set_read_addr(channel, ringbuf->buffer, false);
        dma_channel_set_write_addr(channel, target_addr, false);
    } else {
        dma_channel_set_read_addr(channel, target_addr, false);
        dma_channel_set_write_addr(channel, ringbuf->buffer, false);
    }
    common_hal_rp2pio_dma_set_irq(ringbuf->channel, _irq_handler, ringbuf);
    common_hal_rp2pio_dmaringbuf_flush(ringbuf);
    return true;
}

void common_hal_rp2pio_dmaringbuf_deinit(rp2pio_dmaringbuf_t *ringbuf) {
    if (ringbuf->channel != -1u) {
        peripherals_dma_channel_unclaim(ringbuf->channel);
        ringbuf->channel = -1u;
    }
    if (ringbuf->buffer) {
        gc_free(ringbuf->buffer);
        ringbuf->buffer = NULL;
    }
}

STATIC uint _get_trans_count(rp2pio_dmaringbuf_t *ringbuf) {
    uint trans_count = ringbuf->next_write - ringbuf->next_read;
    if (!ringbuf->tx) {
        trans_count = ringbuf->size - trans_count;
    }
    trans_count = MIN(trans_count, ringbuf->max_transfer_count);
    trans_count >>= ringbuf->transfer_size;
    return trans_count;
}

STATIC uint _get_next(rp2pio_dmaringbuf_t *ringbuf) {
    uint trans_count = dma_channel_hw_addr(ringbuf->channel)->transfer_count;
    uint delta = (ringbuf->trans_count - trans_count) << ringbuf->transfer_size;
    if (ringbuf->tx) {
        ringbuf->next_read += delta;
    } else {
        ringbuf->next_write += delta;
    }
    return trans_count;
}

STATIC void _irq_handler(uint channel, void *context) {
    rp2pio_dmaringbuf_t *ringbuf = context;
    common_hal_rp2pio_dma_acknowledge_irq(ringbuf->channel);
    ringbuf->int_count++;

    uint trans_count = _get_next(ringbuf);
    assert(trans_count == 0);

    trans_count = _get_trans_count(ringbuf);
    ringbuf->trans_count = trans_count;
    if (trans_count) {
        dma_channel_set_trans_count(ringbuf->channel, trans_count, true);
    } else if (ringbuf->handler) {
        ringbuf->handler(ringbuf);
    }
}

void common_hal_rp2pio_dmaringbuf_sync(rp2pio_dmaringbuf_t *ringbuf) {
    if (!ringbuf->trans_count) {
        return;
    }
    common_hal_rp2pio_dma_clear_irq(ringbuf->channel);
    uint trans_count = _get_next(ringbuf);
    ringbuf->trans_count = trans_count;
    common_hal_rp2pio_dma_set_irq(ringbuf->channel, _irq_handler, ringbuf);
}

void common_hal_rp2pio_dmaringbuf_flush(rp2pio_dmaringbuf_t *ringbuf) {
    if (ringbuf->trans_count) {
        return;
    }
    uint trans_count = _get_trans_count(ringbuf);
    if (trans_count) {
        ringbuf->trans_count = trans_count;
        dma_channel_set_trans_count(ringbuf->channel, trans_count, true);
    }
}

size_t common_hal_rp2pio_dmaringbuf_acquire(rp2pio_dmaringbuf_t *ringbuf, void **buf) {
    uint next_read = ringbuf->next_read;
    uint next_write = ringbuf->next_write;
    uint index = next_read;
    uint count = next_write - next_read;
    if (ringbuf->tx) {
        index = next_write;
        count = ringbuf->size - count;
    }
    index &= ringbuf->size - 1u;
    *buf = ringbuf->buffer + index;
    return MIN(count, ringbuf->size - index);
}

void common_hal_rp2pio_dmaringbuf_release(rp2pio_dmaringbuf_t *ringbuf, size_t bufsize) {
    if (ringbuf->tx) {
        ringbuf->next_write += bufsize;
    } else {
        ringbuf->next_read += bufsize;
    }
}

size_t common_hal_rp2pio_dmaringbuf_transfer(rp2pio_dmaringbuf_t *ringbuf, void *buf, size_t bufsize) {
    void *ring;
    size_t result = common_hal_rp2pio_dmaringbuf_acquire(ringbuf, &ring);
    result = MIN(result, bufsize);
    if (buf) {
        if (ringbuf->tx) {
            memcpy(ring, buf, result);
        } else {
            memcpy(buf, ring, result);
        }
        common_hal_rp2pio_dmaringbuf_release(ringbuf, result);
    }
    return result;
}

void common_hal_rp2pio_dmaringbuf_clear(rp2pio_dmaringbuf_t *ringbuf) {
    common_hal_rp2pio_dma_clear_irq(ringbuf->channel);
    dma_channel_abort(ringbuf->channel);
    common_hal_rp2pio_dma_acknowledge_irq(ringbuf->channel);

    ringbuf->next_read = 0;
    ringbuf->next_write = 0;
    if (ringbuf->tx) {
        dma_channel_set_read_addr(ringbuf->channel, ringbuf->buffer, false);
    } else {
        dma_channel_set_write_addr(ringbuf->channel, ringbuf->buffer, false);
    }
    ringbuf->trans_count = 0;
    dma_channel_set_trans_count(ringbuf->channel, 0, false);

    common_hal_rp2pio_dma_set_irq(ringbuf->channel, _irq_handler, ringbuf);

    common_hal_rp2pio_dmaringbuf_set_enabled(ringbuf, true);
    common_hal_rp2pio_dmaringbuf_flush(ringbuf);
}

void common_hal_rp2pio_dmaringbuf_set_enabled(rp2pio_dmaringbuf_t *ringbuf, bool enable) {
    dma_channel_config c = dma_get_channel_config(ringbuf->channel);
    channel_config_set_enable(&c, enable);
    dma_channel_set_config(ringbuf->channel, &c, false);
}

void common_hal_rp2pio_dmaringbuf_set_handler(rp2pio_dmaringbuf_t *ringbuf, rp2pio_dmaringbuf_handler_t handler) {
    common_hal_rp2pio_dma_clear_irq(ringbuf->channel);
    ringbuf->handler = handler;
    common_hal_rp2pio_dma_set_irq(ringbuf->channel, _irq_handler, ringbuf);
}

void common_hal_rp2pio_dmaringbuf_debug(const mp_print_t *print, rp2pio_dmaringbuf_t *ringbuf) {
    mp_printf(print, "dma ringbuf %p\n", ringbuf);
    mp_printf(print, "  tx:          %d\n", ringbuf->tx);
    mp_printf(print, "  buffer       %p\n", ringbuf->buffer);
    mp_printf(print, "  size:        %u\n", ringbuf->size);
    mp_printf(print, "  next_read:   %u (%04x)\n", ringbuf->next_read, ringbuf->next_read & (ringbuf->size - 1u));
    mp_printf(print, "  next_write:  %u (%04x)\n", ringbuf->next_write, ringbuf->next_write & (ringbuf->size - 1u));
    mp_printf(print, "  trans_count: %u\n", ringbuf->trans_count);
    mp_printf(print, "  max_trans_count: %u\n", ringbuf->max_transfer_count);
    mp_printf(print, "  int_count:   %u\n", ringbuf->int_count);

    if (ringbuf->channel != -1u) {
        common_hal_rp2pio_dma_debug(print, ringbuf->channel);
    }
}
