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

/* The IRQ enable bits for each GPIO pad are duplicated by core. Because we are in a regime where
 * all interrupt activity happens on a designated core, we must be careful how these bits are
 * accessed. Fortunately because it is possible to directly access the other core's register, we
 * don't need to switch the core we're already executing on. Therefore it is still possible to call
 * this function from an interrupt context.
 *
 * TODO: similar wrappers for irq_set_mask_enabled and irq_is_enabled.
 */
void pico_gpio_set_irq_enabled(uint gpio, uint32_t events, bool enabled) {
    io_irq_ctrl_hw_t *irq_ctrl_base = &iobank0_hw->proc0_irq_ctrl + INTERRUPT_CORE_NUM;

    // Clear stale events which might cause immediate spurious handler entry
    gpio_acknowledge_irq(gpio, events);

    io_rw_32 *en_reg = &irq_ctrl_base->inte[gpio / 8];
    events <<= 4 * (gpio % 8);

    if (enabled) {
        hw_set_bits(en_reg, events);
    } else {
        hw_clear_bits(en_reg, events);
    }
}

bool pico_gpio_add_handler(uint gpio, pico_gpio_handler_t handler, void *context) {
    taskENTER_CRITICAL();
    bool ret = false;
    if (!pico_gpio_handlers[gpio]) {
        pico_gpio_set_irq_enabled(gpio, 0xf, false);
        pico_gpio_handlers[gpio] = handler;
        pico_gpio_contexts[gpio] = context;
        ret = true;
    }
    taskEXIT_CRITICAL();
    return ret;
}

bool pico_gpio_remove_handler(uint gpio) {
    taskENTER_CRITICAL();
    bool ret = false;
    if (pico_gpio_handlers[gpio]) {
        pico_gpio_set_irq_enabled(gpio, 0xf, false);
        pico_gpio_handlers[gpio] = NULL;
        pico_gpio_contexts[gpio] = NULL;
        ret = true;
    }
    taskEXIT_CRITICAL();
    return ret;
}

#ifndef NDEBUG
#include <stdio.h>

void pico_gpio_debug(uint gpio) {
    check_gpio_param(gpio);
    io_irq_ctrl_hw_t *irq_ctrl_base = &iobank0_hw->proc0_irq_ctrl + INTERRUPT_CORE_NUM;
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
