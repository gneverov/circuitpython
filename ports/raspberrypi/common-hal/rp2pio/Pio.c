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

#include "common-hal/rp2pio/Pio.h"
#include "py/gc.h"
#include "src/rp2_common/hardware_irq/include/hardware/irq.h"

PIO all_pios[NUM_PIOS] = { pio0, pio1 };

typedef struct {
    rp2pio_pio_irq_handler_t handler;
    void *context;
} rp2pio_pio_irq_t;

STATIC rp2pio_pio_irq_t *_irq_table[NUM_PIOS];

STATIC uint8_t _used_pins[NUM_PIOS][NUM_BANK0_GPIOS];

STATIC rp2pio_pio_irq_t *_get_irq_entry(PIO pio, enum pio_interrupt_source source) {
    uint index = pio_get_index(pio);
    return &_irq_table[index][source];
}

STATIC void _irq_handler(PIO pio) {
    for (int i = 0; i < NUM_PIO_INTERRUPT_SOURCES; i++) {
        if (pio->ints0 & (1 << i)) {
            pio_set_irq0_source_enabled(pio, i, false);
            rp2pio_pio_irq_t *entry = _get_irq_entry(pio, i);
            entry->handler(pio, i, entry->context);
        }
    }
}

STATIC void _irq_handler_pio0(void) {
    _irq_handler(pio0);
}

STATIC void _irq_handler_pio1(void) {
    _irq_handler(pio1);
}

STATIC void _cinit_pio(uint pio_index, uint irq, irq_handler_t irq_handler) {
    assert(pio_index < NUM_PIOS);
    PIO pio = all_pios[pio_index];
    rp2pio_pio_irq_t **irq_table = &_irq_table[pio_index];
    if (*irq_table != NULL) {
        return;
    }

    pio_clear_instruction_memory(pio);
    *irq_table = m_new_ll(rp2pio_pio_irq_t, NUM_PIO_INTERRUPT_SOURCES);
    gc_never_free(*irq_table);

    for (uint i = 0; i < NUM_PIO_INTERRUPT_SOURCES; i++) {
        common_hal_rp2pio_pio_clear_irq(pio, i);
    }
    irq_add_shared_handler(irq, irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(irq, true);
}

void common_hal_rp2pio_pio_cinit(void) {
    _cinit_pio(0, PIO0_IRQ_0, _irq_handler_pio0);
    _cinit_pio(1, PIO0_IRQ_1, _irq_handler_pio1);
}

STATIC void _reset_pio(uint pio_index, uint irq, irq_handler_t irq_handler) {
    assert(pio_index < NUM_PIOS);
    PIO pio = all_pios[pio_index];
    rp2pio_pio_irq_t **irq_table = &_irq_table[pio_index];
    if (*irq_table == NULL) {
        return;
    }

    irq_set_enabled(irq, false);
    irq_remove_handler(irq, irq_handler);

    for (uint i = 0; i < NUM_PIO_INTERRUPT_SOURCES; i++) {
        pio_set_irq0_source_enabled(pio, i, false);
    }

    gc_free(*irq_table);
    *irq_table = NULL;
}

void common_hal_rp2pio_pio_reset(void) {
    _reset_pio(0, PIO0_IRQ_0, _irq_handler_pio0);
    _reset_pio(1, PIO0_IRQ_1, _irq_handler_pio1);

    memset(&_used_pins, 0, sizeof(_used_pins));
}

void common_hal_rp2pio_pio_set_irq(PIO pio, enum pio_interrupt_source source, rp2pio_pio_irq_handler_t handler, void *context) {
    rp2pio_pio_irq_t *entry = _get_irq_entry(pio, source);
    entry->handler = handler;
    entry->context = context;
    pio_set_irq0_source_enabled(pio, source, true);
}

void common_hal_rp2pio_pio_clear_irq(PIO pio, enum pio_interrupt_source source) {
    pio_set_irq0_source_enabled(pio, source, false);
    rp2pio_pio_irq_t *entry = _get_irq_entry(pio, source);
    entry->handler = NULL;
    entry->context = NULL;
}

bool common_hal_rp2pio_pio_claim_pin(PIO pio, const mcu_pin_obj_t *pin) {
    uint pio_index = pio_get_index(pio);
    uint pin_num = common_hal_mcu_pin_number(pin);
    uint8_t *used_pin = &_used_pins[pio_index][pin_num];
    if (*used_pin) {
        ++(*used_pin);
        return true;
    }
    if (!common_hal_mcu_pin_is_free(pin)) {
        return false;
    }
    common_hal_mcu_pin_claim(pin);
    ++(*used_pin);
    pio_gpio_init(pio, pin_num);
    return true;
}

void common_hal_rp2pio_pio_unclaim_pin(PIO pio, const mcu_pin_obj_t *pin) {
    uint pio_index = pio_get_index(pio);
    uint pin_num = common_hal_mcu_pin_number(pin);
    uint8_t *used_pin = &_used_pins[pio_index][pin_num];

    --(*used_pin);
    if (!*used_pin) {
        common_hal_reset_pin(pin);
    }
}

void common_hal_rp2pio_pio_debug(const mp_print_t *print, PIO pio) {
    mp_printf(print, "PIO %u\n", pio_get_index(pio));

    uint inte = pio->inte0;
    uint ints = pio->ints0;
    for (uint i = 0; i < NUM_PIO_INTERRUPT_SOURCES; i++) {
        uint bit = 1u << i;
        rp2pio_pio_irq_t *entry = _get_irq_entry(pio, i);
        if ((inte & bit) || (ints & bit) || entry->handler || entry->context) {
            mp_printf(print, "  irq %2d: %d %d %p %p\n", i, inte & bit, ints & bit, entry->handler, entry->context);
        }
    }

    uint8_t *used_pins = _used_pins[pio_get_index(pio)];
    for (uint i = 0; i < NUM_BANK0_GPIOS; i++) {
        if (used_pins[i]) {
            mp_printf(print, "  pin %2d: %u\n", i, used_pins[i]);
        }
    }
}
