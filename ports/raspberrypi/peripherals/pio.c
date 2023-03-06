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

#include "peripherals/pio.h"
#include "py/gc.h"
#include "src/rp2_common/hardware_irq/include/hardware/irq.h"

PIO all_pios[NUM_PIOS] = { pio0, pio1 };

STATIC uint _claimed_sm_mask[NUM_PIOS];
STATIC uint _never_reset_sm_mask[NUM_PIOS];

STATIC uint *_get_claimed_sm_mask(PIO pio) {
    return &_claimed_sm_mask[pio_get_index(pio)];
}

STATIC uint *_get_never_reset_sm_mask(PIO pio) {
    return &_never_reset_sm_mask[pio_get_index(pio)];
}

typedef struct {
    peripherals_pio_irq_handler_t handler;
    void *context;
} peripherals_pio_irq_t;

STATIC peripherals_pio_irq_t _irq_table[NUM_PIOS][NUM_PIO_INTERRUPT_SOURCES];

STATIC uint8_t _used_pins[NUM_PIOS][NUM_BANK0_GPIOS];

STATIC peripherals_pio_irq_t *_get_irq_entry(PIO pio, enum pio_interrupt_source source) {
    assert(source < NUM_PIO_INTERRUPT_SOURCES);
    uint index = pio_get_index(pio);
    return &_irq_table[index][source];
}

STATIC void _irq_handler(PIO pio) {
    for (int i = 0; i < NUM_PIO_INTERRUPT_SOURCES; i++) {
        if (pio->ints0 & (1 << i)) {
            peripherals_pio_irq_t *entry = _get_irq_entry(pio, i);
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

STATIC void _init_pio(uint pio_index, uint irq, irq_handler_t irq_handler) {
    assert(pio_index < NUM_PIOS);
    irq_add_shared_handler(irq, irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    irq_set_enabled(irq, true);
}

void peripherals_pio_init(void) {
    _init_pio(0, PIO0_IRQ_0, _irq_handler_pio0);
    _init_pio(1, PIO0_IRQ_1, _irq_handler_pio1);
}

STATIC void _reset_pio(uint pio_index, uint irq, irq_handler_t irq_handler) {
    assert(pio_index < NUM_PIOS);
    PIO pio = all_pios[pio_index];

    uint *claimed_sm_mask = _get_claimed_sm_mask(pio);
    uint never_reset_sm_mask = *_get_never_reset_sm_mask(pio);
    uint reset_sm_mask = *claimed_sm_mask & ~never_reset_sm_mask;
    for (uint i = 0; i < NUM_PIO_STATE_MACHINES; i++) {
        if (reset_sm_mask & (1u << i)) {
            peripherals_pio_sm_unclaim(pio, i);
        }
    }
    *claimed_sm_mask &= never_reset_sm_mask;
}

void peripherals_pio_reset(void) {
    _reset_pio(0, PIO0_IRQ_0, _irq_handler_pio0);
    _reset_pio(1, PIO0_IRQ_1, _irq_handler_pio1);

    memset(&_used_pins, 0, sizeof(_used_pins));
}

void peripherals_pio_gc_collect(void) {
    for (uint i = 0; i < NUM_PIOS; i++) {
        for (uint j = 0; j < NUM_PIO_INTERRUPT_SOURCES; j++) {
            gc_collect_ptr(_irq_table[i][j].context);
        }
    }
}

void peripherals_pio_set_irq(PIO pio, enum pio_interrupt_source source, peripherals_pio_irq_handler_t handler, void *context) {
    peripherals_pio_irq_t *entry = _get_irq_entry(pio, source);
    entry->handler = handler;
    entry->context = context;
    pio_set_irq0_source_enabled(pio, source, true);
}

void peripherals_pio_clear_irq(PIO pio, enum pio_interrupt_source source) {
    pio_set_irq0_source_enabled(pio, source, false);
    peripherals_pio_irq_t *entry = _get_irq_entry(pio, source);
    entry->handler = NULL;
    entry->context = NULL;
}

bool peripherals_pio_sm_claim(PIO pio, uint *sm) {
    int c = pio_claim_unused_sm(pio, false);
    if (c == -1) {
        return false;
    }

    *sm = c;
    uint bit = 1u << *sm;
    *_get_claimed_sm_mask(pio) |= bit;
    return true;
}

void peripherals_pio_sm_never_reset(PIO pio, uint sm) {
    uint *sm_mask = _get_claimed_sm_mask(pio);
    uint bit = 1u << sm;
    assert(*sm_mask & bit);
    *sm_mask |= bit;
}

void peripherals_pio_sm_unclaim(PIO pio, uint sm) {
    uint *sm_mask = _get_claimed_sm_mask(pio);
    uint bit = 1u << sm;
    assert(*sm_mask & bit);

    if (*sm_mask & bit) {
        peripherals_pio_clear_irq(pio, sm);
        pio_sm_set_enabled(pio, sm, false);
        pio_sm_unclaim(pio, sm);
    }
    *sm_mask &= ~bit;
    *_get_never_reset_sm_mask(pio) &= ~bit;
}

bool peripherals_pio_claim_pin(PIO pio, const mcu_pin_obj_t *pin) {
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

void peripherals_pio_unclaim_pin(PIO pio, const mcu_pin_obj_t *pin) {
    uint pio_index = pio_get_index(pio);
    uint pin_num = common_hal_mcu_pin_number(pin);
    uint8_t *used_pin = &_used_pins[pio_index][pin_num];

    --(*used_pin);
    if (!*used_pin) {
        common_hal_reset_pin(pin);
    }
}

void peripherals_pio_debug(const mp_print_t *print, PIO pio) {
    mp_printf(print, "PIO %u\n", pio_get_index(pio));

    uint inte = pio->inte0;
    uint ints = pio->ints0;
    for (uint i = 0; i < NUM_PIO_INTERRUPT_SOURCES; i++) {
        uint bit = 1u << i;
        peripherals_pio_irq_t *entry = _get_irq_entry(pio, i);
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
