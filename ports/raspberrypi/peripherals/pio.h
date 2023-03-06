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

#pragma once

#include "py/obj.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "src/rp2_common/hardware_pio/include/hardware/pio.h"

#define NUM_PIO_INTERRUPT_SOURCES 12

extern PIO all_pios[NUM_PIOS];

typedef void (*peripherals_pio_irq_handler_t)(PIO pio, enum pio_interrupt_source source, void *context);

void peripherals_pio_init(void);

void peripherals_pio_reset(void);

void peripherals_pio_gc_collect(void);

void peripherals_pio_set_irq(PIO pio, enum pio_interrupt_source source, peripherals_pio_irq_handler_t handler, void *context);

void peripherals_pio_clear_irq(PIO pio, enum pio_interrupt_source source);


bool peripherals_pio_sm_claim(PIO pio, uint *sm);

void peripherals_pio_sm_never_reset(PIO pio, uint sm);

void peripherals_pio_sm_unclaim(PIO pio, uint sm);


bool peripherals_pio_claim_pin(PIO pio, const mcu_pin_obj_t *pin);

// void peripherals_pio_pin_never_reset_pin(uint timer);

void peripherals_pio_unclaim_pin(PIO pio, const mcu_pin_obj_t *pin);

void peripherals_pio_debug(const mp_print_t *print, PIO pio);
