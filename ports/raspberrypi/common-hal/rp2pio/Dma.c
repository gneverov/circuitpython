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

#include "common-hal/rp2pio/Dma.h"
#include "py/gc.h"
#include "src/rp2_common/hardware_irq/include/hardware/irq.h"

typedef struct {
    rp2pio_dma_irq_handler_t handler;
    void *context;
} rp2pio_dma_irq_t;

STATIC rp2pio_dma_irq_t *_irq_table = NULL;

STATIC rp2pio_dma_irq_t *_get_irq_entry(uint channel) {
    return &_irq_table[channel];
}

STATIC void _irq_handler(void) {
    for (uint i = 0; i < NUM_DMA_CHANNELS; i++) {
        if (dma_channel_get_irq1_status(i)) {
            dma_channel_set_irq1_enabled(i, false);
            rp2pio_dma_irq_t *entry = _get_irq_entry(i);
            entry->handler(i, entry->context);
        }
    }
}

void common_hal_rp2pio_dma_cinit(void) {
    if (_irq_table != NULL) {
        return;
    }

    _irq_table = m_new_ll(rp2pio_dma_irq_t, NUM_DMA_CHANNELS);
    gc_never_free(_irq_table);
    for (uint i = 0; i < NUM_DMA_CHANNELS; i++) {
        common_hal_rp2pio_dma_clear_irq(i);
    }

    irq_add_shared_handler(DMA_IRQ_1, _irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_1, true);
}

void common_hal_rp2pio_dma_set_irq(uint channel, rp2pio_dma_irq_handler_t handler, void *context) {
    rp2pio_dma_irq_t *entry = _get_irq_entry(channel);
    entry->handler = handler;
    entry->context = context;
    dma_channel_set_irq1_enabled(channel, true);
}

void common_hal_rp2pio_dma_clear_irq(uint channel) {
    dma_channel_set_irq1_enabled(channel, false);
    rp2pio_dma_irq_t *entry = _get_irq_entry(channel);
    entry->handler = NULL;
    entry->context = NULL;
}

void *common_hal_rp2pio_dma_alloc_aligned(int size_bits, bool long_lived) {
    size_t size = 1 << size_bits;
    void *ptr = gc_alloc(size, 0, long_lived);
    if (!ptr) {
        return NULL;
    }

    size_t offset = -(size_t)ptr & (size - 1);
    if (offset) {
        void *tmp = gc_realloc(ptr, offset, false);
        assert(tmp);
        ptr = gc_alloc(size, 0, long_lived);
        gc_free(tmp);
        if (!ptr) {
            return NULL;
        }
    }

    if ((size_t)ptr & (size - 1)) {
        gc_free(ptr);
        return NULL;
    }
    return ptr;
}

void common_hal_rp2pio_dma_debug(const mp_print_t *print, uint channel) {
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

    rp2pio_dma_irq_t *entry = _get_irq_entry(channel);
    mp_printf(print, "  handler:     %p\n", entry->handler);
    mp_printf(print, "  context:     %p\n", entry->context);
}
