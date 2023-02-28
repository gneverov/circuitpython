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

#include "py/mpprint.h"
#include "src/rp2_common/hardware_dma/include/hardware/dma.h"

typedef void (*rp2pio_dma_irq_handler_t)(uint channel, void *context);

void common_hal_rp2pio_dma_cinit(void);

void peripherals_dma_reset(void);

void common_hal_rp2pio_dma_set_irq(uint channel, rp2pio_dma_irq_handler_t handler, void *context);

void common_hal_rp2pio_dma_clear_irq(uint channel);

void common_hal_rp2pio_dma_acknowledge_irq(uint channel);


bool peripherals_dma_channel_claim(uint *channel);

void peripherals_dma_channel_never_reset(uint channel);

void peripherals_dma_channel_unclaim(uint channel);


bool peripherals_dma_timer_claim(uint *timer);

void peripherals_dma_timer_never_reset(uint timer);

void peripherals_dma_timer_unclaim(uint timer);


void *common_hal_rp2pio_dma_alloc_aligned(int size_bits, bool long_lived);

void common_hal_rp2pio_dma_debug(const mp_print_t *print, uint channel);
