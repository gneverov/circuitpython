// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "freertos/interrupts.h"

#include "hardware/irq.h"


void pico_irq_set_enabled(uint num, bool enabled) {
    UBaseType_t save = set_interrupt_core_affinity();
    irq_set_enabled(num, enabled);
    clear_interrupt_core_affinity(save);
}

void pico_irq_remove_handler(uint num, irq_handler_t handler) {
    UBaseType_t save = set_interrupt_core_affinity();
    irq_remove_handler(num, handler);
    clear_interrupt_core_affinity(save);
}
