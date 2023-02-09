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

STATIC void _sync(rp2pio_dmaringbuf_t *ringbuf);
STATIC void _trigger(rp2pio_dmaringbuf_t *ringbuf);
STATIC void _irq_handler(uint channel, void *context);

STATIC uint _used_channel_mask;

void common_hal_rp2pio_dmaringbuf_reset(void) {
    if (_used_channel_mask == 0u) {
        return;
    }

    // abort
    dma_hw->abort = _used_channel_mask;
    while (dma_hw->abort) {
        tight_loop_contents();
    }

    // acknowledge irq
    dma_hw->ints1 = _used_channel_mask;

    dma_unclaim_mask(_used_channel_mask);
    _used_channel_mask = 0u;
}

void common_hal_rp2pio_dmaringbuf_init(rp2pio_dmaringbuf_t *ringbuf, bool tx) {
    ringbuf->channel = -1;
    ringbuf_init(&ringbuf->ringbuf, NULL, 0);
    ringbuf->trans_count = 0;
    ringbuf->tx = tx;
    ringbuf->transfer_size = DMA_SIZE_8;
    ringbuf->int_count = 0;
}

bool common_hal_rp2pio_dmaringbuf_alloc(rp2pio_dmaringbuf_t *ringbuf, uint ring_size_bits, uint dreq, enum dma_channel_transfer_size transfer_size, bool bswap, volatile void *target_addr) {
    assert(ring_size_bits >= 4);

    int channel = dma_claim_unused_channel(false);
    if (channel == -1) {
        errno = MP_EBUSY;
        return false;
    }
    _used_channel_mask |= 1u << channel;

    void *ring_addr = common_hal_rp2pio_dma_alloc_aligned(ring_size_bits, false);
    if (!ring_addr) {
        errno = MP_ENOMEM;
        return false;
    }

    ringbuf->channel = channel;
    ringbuf_init(&ringbuf->ringbuf, ring_addr, 1 << ring_size_bits);
    ringbuf->transfer_size = transfer_size;
    ringbuf->trans_count = 0;

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
        dma_channel_set_read_addr(channel, ring_addr, false);
        dma_channel_set_write_addr(channel, target_addr, false);
    } else {
        dma_channel_set_read_addr(channel, target_addr, false);
        dma_channel_set_write_addr(channel, ring_addr, false);
    }
    _trigger(ringbuf);
    return true;
}

void common_hal_rp2pio_dmaringbuf_deinit(rp2pio_dmaringbuf_t *ringbuf) {
    if (ringbuf->channel != -1u) {
        _sync(ringbuf);
        dma_channel_abort(ringbuf->channel);
        common_hal_rp2pio_dma_acknowledge_irq(ringbuf->channel);
        dma_channel_unclaim(ringbuf->channel);
        _used_channel_mask &= ~(1u << ringbuf->channel);
        ringbuf->channel = -1u;
    }
    ringbuf_deinit(&ringbuf->ringbuf);
}

STATIC void _sync(rp2pio_dmaringbuf_t *ringbuf) {
    common_hal_rp2pio_dma_clear_irq(ringbuf->channel);

    uint trans_count = dma_channel_hw_addr(ringbuf->channel)->transfer_count;
    ptrdiff_t delta = (ringbuf->trans_count - trans_count) << ringbuf->transfer_size;
    if (ringbuf->tx) {
        ringbuf->ringbuf.next_read += delta;
        ringbuf->ringbuf.next_read %= ringbuf->ringbuf.size;
        ringbuf->ringbuf.used -= delta;
    } else {
        ringbuf->ringbuf.next_write += delta;
        ringbuf->ringbuf.next_write %= ringbuf->ringbuf.size;
        ringbuf->ringbuf.used += delta;
    }
    ringbuf->trans_count = trans_count;
}

STATIC void _trigger(rp2pio_dmaringbuf_t *ringbuf) {
    if (!ringbuf->trans_count) {
        uint32_t trans_count = ringbuf->tx ? ringbuf->ringbuf.used : ringbuf->ringbuf.size - ringbuf->ringbuf.used;
        trans_count = MIN(trans_count, ringbuf->ringbuf.size / 2);
        trans_count >>= ringbuf->transfer_size;
        if (trans_count) {
            dma_channel_set_trans_count(ringbuf->channel, trans_count, true);
            ringbuf->trans_count = trans_count;
        }
    }
    if (ringbuf->trans_count) {
        common_hal_rp2pio_dma_set_irq(ringbuf->channel, _irq_handler, ringbuf);
    }
}

STATIC void _irq_handler(uint channel, void *context) {
    rp2pio_dmaringbuf_t *ringbuf = context;
    common_hal_rp2pio_dma_acknowledge_irq(ringbuf->channel);
    ringbuf->int_count++;

    _sync(ringbuf);
    _trigger(ringbuf);
}

size_t common_hal_rp2pio_dmaringbuf_transfer(rp2pio_dmaringbuf_t *ringbuf, void *buf, size_t bufsize) {
    _sync(ringbuf);
    size_t result;
    if (buf) {
        result = ringbuf->tx ? ringbuf_put_n(&ringbuf->ringbuf, buf, bufsize) : ringbuf_get_n(&ringbuf->ringbuf, buf, bufsize);
    } else {
        result = ringbuf->tx ? ringbuf_num_empty(&ringbuf->ringbuf) : ringbuf_num_filled(&ringbuf->ringbuf);
        result = MIN(result, bufsize);
    }
    _trigger(ringbuf);
    return result;
}

void common_hal_rp2pio_dmaringbuf_clear(rp2pio_dmaringbuf_t *ringbuf) {
    _sync(ringbuf);
    dma_channel_abort(ringbuf->channel);
    common_hal_rp2pio_dma_acknowledge_irq(ringbuf->channel);
    ringbuf->trans_count = 0;
    dma_channel_set_trans_count(ringbuf->channel, 0, false);

    void *ring_addr = ringbuf->ringbuf.buf;
    ringbuf_clear(&ringbuf->ringbuf);
    if (ringbuf->tx) {
        dma_channel_set_read_addr(ringbuf->channel, ring_addr, false);
    } else {
        dma_channel_set_write_addr(ringbuf->channel, ring_addr, false);
    }
    common_hal_rp2pio_dmaringbuf_set_enabled(ringbuf, true);
    _trigger(ringbuf);
}

void common_hal_rp2pio_dmaringbuf_set_enabled(rp2pio_dmaringbuf_t *ringbuf, bool enable) {
    dma_channel_config c = dma_get_channel_config(ringbuf->channel);
    channel_config_set_enable(&c, enable);
    dma_channel_set_config(ringbuf->channel, &c, false);
}

void common_hal_rp2pio_dmaringbuf_debug(const mp_print_t *print, rp2pio_dmaringbuf_t *ringbuf) {
    mp_printf(print, "dma ringbuf %p\n", ringbuf);
    mp_printf(print, "  tx:          %d\n", ringbuf->tx);
    mp_printf(print, "  trans_count: %u\n", ringbuf->trans_count);
    mp_printf(print, "  int_count:   %u\n", ringbuf->int_count);

    common_hal_rp2pio_dma_debug(print, ringbuf->channel);

    ringbuf_t *sw_ringbuf = &ringbuf->ringbuf;
    mp_printf(print, "ringbuf %p\n", sw_ringbuf);
    mp_printf(print, "  buf:        %p\n", sw_ringbuf->buf);
    mp_printf(print, "  size:       %u\n", sw_ringbuf->size);
    mp_printf(print, "  used:       %u\n", sw_ringbuf->used);
    mp_printf(print, "  next_read:  %u\n", sw_ringbuf->next_read);
    mp_printf(print, "  next_write: %u\n", sw_ringbuf->next_write);
}
