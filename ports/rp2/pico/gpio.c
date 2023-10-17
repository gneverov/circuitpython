// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "pico/gpio.h"

#include "hardware/gpio.h"


static pico_gpio_handler_t pico_gpio_handlers[NUM_BANK0_GPIOS];
static void *pico_gpio_contexts[NUM_BANK0_GPIOS];

static void pico_gpio_irq_handler(uint gpio, uint32_t event_mask) {
    assert(pico_gpio_handlers[gpio]);
    pico_gpio_handlers[gpio](gpio, event_mask, pico_gpio_contexts[gpio]);
}

void pico_gpio_init(void) {
    gpio_set_irq_callback(pico_gpio_irq_handler);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

void pico_gpio_set_irq(uint gpio, pico_gpio_handler_t handler, void *context) {
    pico_gpio_handlers[gpio] = handler;
    pico_gpio_contexts[gpio] = context;
}

void pico_gpio_clear_irq(uint gpio) {
    gpio_set_irq_enabled(gpio, 0xf, false);
    pico_gpio_handlers[gpio] = NULL;
    pico_gpio_contexts[gpio] = NULL;
}

#ifndef NDEBUG
#include <stdio.h>

static uint32_t pico_gpio_inte(uint gpio) {
    // Separate mask/force/status per-core, so check which core called, and
    // set the relevant IRQ controls.
    io_irq_ctrl_hw_t *irq_ctrl_base = get_core_num() ?
        &iobank0_hw->proc1_irq_ctrl : &iobank0_hw->proc0_irq_ctrl;
    io_rw_32 events = irq_ctrl_base->inte[gpio / 8];
    events >>= 4 * (gpio % 8);
    return events;
}

void pico_gpio_debug(uint gpio) {
    check_gpio_param(gpio);
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
    printf("  inte:        0x%02lx\n", pico_gpio_inte(gpio));
    printf("  ints:        0x%02lx\n", gpio_get_irq_event_mask(gpio));
    printf("  handler:     %p\n", pico_gpio_handlers[gpio]);
    printf("  context:     %p\n", pico_gpio_contexts[gpio]);
}
#endif
