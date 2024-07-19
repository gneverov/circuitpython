// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "hardware/irq.h"

#include "freertos/interrupts.h"

#include "pico/dma.h"


static pico_dma_handler_t pico_dma_handlers[NUM_DMA_CHANNELS];
static void *pico_dma_contexts[NUM_DMA_CHANNELS];

static void pico_dma_irq_handler(void) {
    for (uint channel = 0; channel < NUM_DMA_CHANNELS; channel++) {
        if (dma_channel_get_irq1_status(channel)) {
            assert(pico_dma_handlers[channel]);
            pico_dma_handlers[channel](channel, pico_dma_contexts[channel]);
        }
    }
}

__attribute__((constructor))
void pico_dma_init(void) {
    assert(check_interrupt_core_affinity());
    irq_add_shared_handler(DMA_IRQ_1, pico_dma_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(DMA_IRQ_1, true);
}

void pico_dma_set_irq(uint channel, pico_dma_handler_t handler, void *context) {
    pico_dma_handlers[channel] = handler;
    pico_dma_contexts[channel] = context;
    dma_channel_set_irq1_enabled(channel, true);
}

void pico_dma_clear_irq(uint channel) {
    dma_channel_set_irq1_enabled(channel, false);
    pico_dma_handlers[channel] = NULL;
    pico_dma_contexts[channel] = NULL;
}

void pico_dma_acknowledge_irq(uint channel) {
    dma_channel_acknowledge_irq1(channel);
}

#ifndef NDEBUG
#include <stdio.h>

void pico_dma_debug(uint channel) {
    check_dma_channel_param(channel);
    dma_channel_hw_t *hw = &dma_hw->ch[channel];
    printf("dma channel %u\n", channel);
    printf("  read_addr:   %p\n", (void *)hw->read_addr);
    printf("  write_addr:  %p\n", (void *)hw->write_addr);
    printf("  trans_count: %lu\n", hw->transfer_count);
    printf("  enabled:     %d\n", !!(hw->ctrl_trig & 1));
    printf("  ctrl:        %08lx\n", hw->ctrl_trig);

    struct dma_debug_hw_channel *debug_hw = &dma_debug_hw->ch[channel];
    printf("  ctrdeq:      %lu\n", debug_hw->ctrdeq);
    printf("  tcr:         %lu\n", debug_hw->tcr);

    uint bit = 1u << channel;
    printf("  inte:        %d\n", !!(dma_hw->inte1 & bit));
    printf("  ints:        %d\n", !!(dma_hw->ints1 & bit));

    printf("  handler:     %p\n", pico_dma_handlers[channel]);
    printf("  context:     %p\n", pico_dma_contexts[channel]);
}
#endif
