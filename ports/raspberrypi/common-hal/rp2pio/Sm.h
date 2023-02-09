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

#include "common-hal/rp2pio/DmaRingBuf.h"
#include "common-hal/rp2pio/Pio.h"
#include "common-hal/rp2pio/PioSlice.h"
#include "py/obj.h"
#include "src/rp2_common/hardware_pio/include/hardware/pio.h"

typedef struct {
    mp_obj_base_t base;
    rp2pio_pioslice_obj_t *pio_slice;
    uint sm;
    pio_sm_config config;

    rp2pio_dmaringbuf_t rx_ringbuf;
    rp2pio_dmaringbuf_t tx_ringbuf;

    mp_obj_t rx_futures;
    mp_obj_t tx_futures;
    bool rx_waiting;
    bool tx_waiting;
} rp2pio_sm_obj_t;

bool common_hal_rp2pio_sm_init(rp2pio_sm_obj_t *self, const mp_obj_type_t *type, rp2pio_pioslice_obj_t *pio_slice, uint sm);

void common_hal_rp2pio_sm_deinit(rp2pio_sm_obj_t *self);

bool common_hal_rp2pio_sm_set_pins(rp2pio_sm_obj_t *self, int pin_type, uint base, uint count);

bool common_hal_rp2pio_sm_set_pulls(rp2pio_sm_obj_t *self, uint mask, uint up, uint down);

float common_hal_rp2pio_sm_set_frequency(rp2pio_sm_obj_t *self, float freq);

void common_hal_rp2pio_sm_set_wrap(rp2pio_sm_obj_t *self, uint wrap_target, uint wrap);

void common_hal_rp2pio_sm_set_shift(rp2pio_sm_obj_t *self, bool out, bool shift_right, bool _auto, uint threshold);

bool common_hal_rp2pio_sm_configure_fifo(rp2pio_sm_obj_t *self, uint ring_size_bits, bool tx, enum dma_channel_transfer_size transfer_size, bool bswap);

void common_hal_rp2pio_sm_reset(rp2pio_sm_obj_t *self, uint initial_pc);

bool common_hal_rp2pio_sm_begin_wait(rp2pio_sm_obj_t *self, bool tx, rp2pio_pio_irq_handler_t handler, void *context);

void common_hal_rp2pio_sm_end_wait(rp2pio_sm_obj_t *self, bool tx);

bool common_hal_rp2pio_sm_tx_from_source(enum pio_interrupt_source source, uint sm);

void common_hal_rp2pio_sm_debug(const mp_print_t *print, rp2pio_sm_obj_t *self);
