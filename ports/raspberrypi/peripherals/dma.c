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

#include "peripherals/dma.h"
#include "py/gc.h"
#include "src/rp2_common/hardware_irq/include/hardware/irq.h"

STATIC uint _claimed_channel_mask;
STATIC uint _never_reset_channel_mask;

STATIC uint _claimed_timer_mask;
STATIC uint _never_reset_timer_mask;

typedef struct {
    peripherals_dma_irq_handler_t handler;
    void *context;
} peripherals_dma_irq_t;

STATIC peripherals_dma_irq_t _irq_table[NUM_DMA_CHANNELS];

STATIC peripherals_dma_irq_t *_get_irq_entry(uint channel) {
    assert(channel < NUM_DMA_CHANNELS);
    return &_irq_table[channel];
}

STATIC void _irq_handler(void) {
    for (uint i = 0; i < NUM_DMA_CHANNELS; i++) {
        if (dma_channel_get_irq1_status(i)) {
            peripherals_dma_irq_t *entry = _get_irq_entry(i);
            entry->handler(i, entry->context);
        }
    }
}

void peripherals_dma_init(void) {
    irq_add_shared_handler(DMA_IRQ_1, _irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_1, true);
}

void peripherals_dma_reset(void) {
    uint reset_channel_mask = _claimed_channel_mask & ~_never_reset_channel_mask;
    for (uint i = 0; i < NUM_DMA_CHANNELS; i++) {
        if (reset_channel_mask & (1u << i)) {
            peripherals_dma_channel_unclaim(i);
        }
    }
    _claimed_channel_mask &= _never_reset_channel_mask;

    uint reset_timer_mask = _claimed_timer_mask & ~_never_reset_timer_mask;
    for (uint i = 0; i < NUM_DMA_TIMERS; i++) {
        if (reset_timer_mask & (1u << i)) {
            peripherals_dma_timer_unclaim(i);
        }
    }
    _claimed_timer_mask &= _never_reset_timer_mask;
}

void peripherals_dma_gc_collect(void) {
    for (uint i = 0; i < NUM_DMA_CHANNELS; i++) {
        gc_collect_ptr(_irq_table[i].context);
    }
}

bool peripherals_dma_channel_claim(uint *channel) {
    int c = dma_claim_unused_channel(false);
    if (c == -1) {
        return false;
    }

    *channel = c;
    uint bit = 1u << *channel;
    _claimed_channel_mask |= bit;
    return true;
}

void peripherals_dma_channel_never_reset(uint channel) {
    uint bit = 1u << channel;
    assert(_claimed_channel_mask & bit);
    _never_reset_channel_mask |= bit;
}

void peripherals_dma_channel_unclaim(uint channel) {
    uint bit = 1u << channel;
    assert(_claimed_channel_mask & bit);

    if (_claimed_channel_mask & bit) {
        peripherals_dma_clear_irq(channel);
        dma_channel_abort(channel);
        peripherals_dma_acknowledge_irq(channel);
        dma_channel_unclaim(channel);
    }
    _claimed_channel_mask &= ~bit;
    _never_reset_channel_mask &= ~bit;
}

bool peripherals_dma_timer_claim(uint *timer) {
    int t = dma_claim_unused_timer(false);
    if (t == -1) {
        return false;
    }

    *timer = t;
    uint bit = 1u << *timer;
    _claimed_timer_mask |= bit;
    return true;
}

void peripherals_dma_timer_never_reset(uint timer) {
    uint bit = 1u << timer;
    assert(_claimed_timer_mask & bit);
    _never_reset_timer_mask |= bit;
}

void peripherals_dma_timer_unclaim(uint timer) {
    uint bit = 1u << timer;
    assert(_claimed_timer_mask & bit);

    if (_claimed_timer_mask & bit) {
        dma_timer_unclaim(timer);
    }
    _claimed_timer_mask &= ~bit;
    _never_reset_timer_mask &= ~bit;
}

void peripherals_dma_set_irq(uint channel, peripherals_dma_irq_handler_t handler, void *context) {
    peripherals_dma_irq_t *entry = _get_irq_entry(channel);
    entry->handler = handler;
    entry->context = context;
    dma_channel_set_irq1_enabled(channel, true);
}

void peripherals_dma_clear_irq(uint channel) {
    dma_channel_set_irq1_enabled(channel, false);
    peripherals_dma_irq_t *entry = _get_irq_entry(channel);
    entry->handler = NULL;
    entry->context = NULL;
}

void peripherals_dma_acknowledge_irq(uint channel) {
    dma_channel_acknowledge_irq1(channel);
}

void *peripherals_dma_alloc_aligned(int size_bits, bool long_lived) {
    size_t size = 1 << size_bits;
    void *ptr = gc_alloc(2 * size, 0, long_lived);
    if (!ptr) {
        return NULL;
    }

    size_t offset = -(size_t)ptr & (size - 1);
    if (offset) {
        void *tmp = gc_realloc(ptr, offset, false);
        assert(tmp);
        void *head = gc_alloc(size, 0, long_lived);
        ptr = head;
        while ((size_t)ptr & (size - 1)) {
            ptr = *(void **)ptr = gc_alloc(size, 0, long_lived);
        }
        gc_free(tmp);
        while (head != ptr) {
            tmp = *(void **)head;
            gc_free(head);
            head = tmp;
        }
    }

    return ptr;
}

void peripherals_dma_debug(const mp_print_t *print, uint channel) {
    check_dma_channel_param(channel);
    dma_channel_hw_t *hw = &dma_hw->ch[channel];
    mp_printf(print, "dma channel %u\n", channel);
    mp_printf(print, "  read_addr:   %p\n", hw->read_addr);
    mp_printf(print, "  write_addr:  %p\n", hw->write_addr);
    mp_printf(print, "  trans_count: %u\n", hw->transfer_count);
    mp_printf(print, "  ctrl:        %08x\n", hw->ctrl_trig);

    struct dma_debug_hw_channel *debug_hw = &dma_debug_hw->ch[channel];
    mp_printf(print, "  ctrdeq:      %u\n", debug_hw->ctrdeq);
    mp_printf(print, "  tcr:         %u\n", debug_hw->tcr);

    uint bit = 1u << channel;
    mp_printf(print, "  inte:        %d\n", !!(dma_hw->inte1 & bit));
    mp_printf(print, "  ints:        %d\n", !!(dma_hw->ints1 & bit));

    peripherals_dma_irq_t *entry = _get_irq_entry(channel);
    mp_printf(print, "  handler:     %p\n", entry->handler);
    mp_printf(print, "  context:     %p\n", entry->context);
}
