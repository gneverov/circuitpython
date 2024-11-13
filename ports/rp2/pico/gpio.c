// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "pico/gpio.h"

#include "hardware/gpio.h"

#include "freertos/interrupts.h"
#include "semphr.h"

static pico_gpio_handler_t pico_gpio_handlers[NUM_BANK0_GPIOS];
static void *pico_gpio_contexts[NUM_BANK0_GPIOS];

static void pico_gpio_irq_handler(uint gpio, uint32_t event_mask) {
    assert(pico_gpio_handlers[gpio]);
    pico_gpio_handlers[gpio](gpio, event_mask, pico_gpio_contexts[gpio]);
}

__attribute__((constructor, visibility("hidden")))
void pico_gpio_init(void) {
    assert(check_interrupt_core_affinity());
    gpio_set_irq_callback(pico_gpio_irq_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

void pico_gpio_set_irq_enabled(uint gpio, uint32_t events, bool enabled) {
    UBaseType_t save = set_interrupt_core_affinity();
    gpio_set_irq_enabled(gpio, events, enabled);
    clear_interrupt_core_affinity(save);
}

bool pico_gpio_add_handler(uint gpio, pico_gpio_handler_t handler, void *context) {
    UBaseType_t save = set_interrupt_core_affinity();
    bool ret = false;
    if (!pico_gpio_handlers[gpio]) {
        gpio_set_irq_enabled(gpio, 0xf, false);
        pico_gpio_handlers[gpio] = handler;
        pico_gpio_contexts[gpio] = context;
        ret = true;
    }
    clear_interrupt_core_affinity(save);
    return ret;
}

bool pico_gpio_remove_handler(uint gpio) {
    UBaseType_t save = set_interrupt_core_affinity();
    bool ret = false;
    if (pico_gpio_handlers[gpio]) {
        gpio_set_irq_enabled(gpio, 0xf, false);
        pico_gpio_handlers[gpio] = NULL;
        pico_gpio_contexts[gpio] = NULL;
        ret = true;
    }
    clear_interrupt_core_affinity(save);
    return ret;
}

#ifndef NDEBUG
#include <stdio.h>

void pico_gpio_debug(uint gpio) {
    check_gpio_param(gpio);
    io_bank0_irq_ctrl_hw_t *irq_ctrl_base = &io_bank0_hw->proc0_irq_ctrl + INTERRUPT_CORE_NUM;
    printf("gpio %u\n", gpio);
    printf("  function:    %d\n", gpio_get_function(gpio));
    printf("  pulls:       ");
    if (gpio_is_pulled_up(gpio)) {
        printf("up ");
    }
    if (gpio_is_pulled_down(gpio)) {
        printf("down ");
    }
    printf("\n");
    printf("  dir:         %s\n", gpio_is_dir_out(gpio) ? "out" : "in");
    printf("  value:       %d\n", gpio_get(gpio));
    uint32_t events = irq_ctrl_base->inte[gpio / 8];
    events >>= 4 * (gpio % 8);
    printf("  inte:        0x%02lx\n", events & 0xf);
    uint32_t status = irq_ctrl_base->ints[gpio / 8];
    status >>= 4 * (gpio % 8);
    printf("  ints:        0x%02lx\n", status & 0xf);
    printf("  handler:     %p\n", pico_gpio_handlers[gpio]);
    printf("  context:     %p\n", pico_gpio_contexts[gpio]);
}
#endif
