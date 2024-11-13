// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "hardware/irq.h"

#include "pico/pio.h"

#include "freertos/interrupts.h"


static pico_pio_handler_t pico_pio_handlers[NUM_PIOS][NUM_PIO_INTERRUPT_SOURCES];
static void *pico_pio_contexts[NUM_PIOS][NUM_PIO_INTERRUPT_SOURCES];


PIO pico_pio(uint pio_index) {
    valid_params_if(HARDWARE_PIO, pio_index < NUM_PIOS);
    return PIO_INSTANCE(pio_index);
}

static void pico_pio_irq_handler(uint pio_index, uint irq) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    PIO pio = pico_pio(pio_index);
    for (int i = 0; i < NUM_PIO_INTERRUPT_SOURCES; i++) {
        if (pio->ints0 & (1 << i)) {
            assert(pico_pio_handlers[pio_index][i]);
            pico_pio_handlers[pio_index][i](pio, i, pico_pio_contexts[pio_index][i], &xHigherPriorityTaskWoken);
        }
    }
}

static void pico_pio0_irq0_handler(void) {
    pico_pio_irq_handler(0, PIO0_IRQ_0);
}

static void pico_pio1_irq0_handler(void) {
    pico_pio_irq_handler(1, PIO1_IRQ_0);
}

static void pico_pio_irq_init(uint pio_index, uint irq, irq_handler_t irq_handler) {
    assert(pio_index < NUM_PIOS);
    irq_add_shared_handler(irq, irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(irq, true);
}

__attribute__((constructor, visibility("hidden")))
void pico_pio_init(void) {
    assert(check_interrupt_core_affinity());
    pico_pio_irq_init(0, PIO0_IRQ_0, pico_pio0_irq0_handler);
    pico_pio_irq_init(1, PIO1_IRQ_0, pico_pio1_irq0_handler);
}

void pico_pio_set_irq(PIO pio, enum pio_interrupt_source source, pico_pio_handler_t handler, void *context) {
    uint pio_index = pio_get_index(pio);
    UBaseType_t save = set_interrupt_core_affinity();
    pio_set_irq0_source_enabled(pio, source, false);
    pico_pio_handlers[pio_index][source] = handler;
    pico_pio_contexts[pio_index][source] = context;
    pio_set_irq0_source_enabled(pio, source, true);
    clear_interrupt_core_affinity(save);
}

void pico_pio_clear_irq(PIO pio, enum pio_interrupt_source source) {
    uint pio_index = pio_get_index(pio);
    UBaseType_t save = set_interrupt_core_affinity();
    pio_set_irq0_source_enabled(pio, source, false);
    pico_pio_handlers[pio_index][ source] = NULL;
    pico_pio_contexts[pio_index][source] = NULL;
    clear_interrupt_core_affinity(save);
}

#ifndef NDEBUG
#include <stdio.h>

void pico_pio_debug(PIO pio) {
    uint pio_index = pio_get_index(pio);
    printf("PIO %u\n", pio_get_index(pio));

    uint inte = pio->inte0;
    uint ints = pio->ints0;
    for (uint i = 0; i < NUM_PIO_INTERRUPT_SOURCES; i++) {
        uint bit = 1u << i;
        pico_pio_handler_t handler = pico_pio_handlers[pio_index][i];
        void *context = pico_pio_contexts[pio_index][i];
        if ((inte & bit) || (ints & bit) || handler || context) {
            printf("  irq %2d: %d %d %p %p\n", i, inte & bit, ints & bit, handler, context);
        }
    }
}
#endif
