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

#include "common-hal/rp2pio/Sm.h"
#include "common-hal/microcontroller/Pin.h"
#include "shared-module/_asyncio/Loop.h"
#include "common-hal/rp2pio/Pio.h"
#include "py/mperrno.h"
#include "src/rp2_common/hardware_clocks/include/hardware/clocks.h"
#include "src/rp2_common/hardware_gpio/include/hardware/gpio.h"

bool common_hal_rp2pio_sm_init(rp2pio_sm_obj_t *self, const mp_obj_type_t *type, rp2pio_pioslice_obj_t *pio_slice, uint sm) {
    self->base.type = type;
    self->pio_slice = pio_slice;
    self->sm = sm;
    self->config = pio_get_default_sm_config();
    sm_config_set_wrap(&self->config, pio_slice->loaded_offset, pio_slice->loaded_offset + pio_slice->program.length - 1);
    pio_sm_init(pio_slice->pio, sm, pio_slice->loaded_offset, &self->config);

    common_hal_rp2pio_dmaringbuf_init(&self->rx_ringbuf, false);
    common_hal_rp2pio_dmaringbuf_init(&self->tx_ringbuf, true);
    self->rx_waiting = false;
    self->tx_waiting = false;
    if (!common_hal_rp2pio_sm_configure_fifo(self, 4, false, DMA_SIZE_8, false)) {
        return false;
    }
    if (!common_hal_rp2pio_sm_configure_fifo(self, 4, true, DMA_SIZE_8, false)) {
        return false;
    }

    self->rx_futures = mp_obj_new_list(0, NULL);
    self->tx_futures = mp_obj_new_list(0, NULL);
    return true;
}

void common_hal_rp2pio_sm_deinit(rp2pio_sm_obj_t *self) {
    if (self->sm != -1u) {
        common_hal_rp2pio_sm_end_wait(self, true);
        common_hal_rp2pio_dmaringbuf_deinit(&self->tx_ringbuf);
        common_hal_rp2pio_pioslice_release_sm(self->pio_slice, self->sm);
        common_hal_rp2pio_sm_end_wait(self, false);
        common_hal_rp2pio_dmaringbuf_deinit(&self->rx_ringbuf);
        self->sm = -1u;

        if (self->pio_slice->sm_mask == 0u) {
            common_hal_rp2pio_pioslice_deinit(self->pio_slice);
        }
        self->pio_slice = NULL;
    }
}

bool common_hal_rp2pio_sm_set_pins(rp2pio_sm_obj_t *self, int pin_type, uint base, uint count) {
    uint mask = ((1u << count) - 1) << base;
    if ((self->pio_slice->pin_mask & mask) != mask) {
        return false;
    }
    switch (pin_type) {
        case 0:
            sm_config_set_out_pins(&self->config, base, count);
            break;
        case 1:
            sm_config_set_set_pins(&self->config, base, count);
            break;
        case 2:
            sm_config_set_in_pins(&self->config, base);
            gpio_set_dir_in_masked(mask);
            break;
        case 3:
            sm_config_set_sideset_pins(&self->config, base);
            break;
        case 4:
            sm_config_set_jmp_pin(&self->config, base);
            gpio_set_dir_in_masked(mask);
            break;
    }
    return true;
}

bool common_hal_rp2pio_sm_set_pulls(rp2pio_sm_obj_t *self, uint mask, uint up, uint down) {
    if ((self->pio_slice->pin_mask & mask) != mask) {
        return false;
    }
    for (uint pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        uint bit = 1u << pin;
        if (mask & bit) {
            gpio_set_pulls(pin, up & bit, down & bit);
        }
    }
    return true;
}

float common_hal_rp2pio_sm_set_frequency(rp2pio_sm_obj_t *self, float freq) {
    uint32_t sysclk = clock_get_hz(clk_sys);
    sm_config_set_clkdiv(&self->config, sysclk / freq);
    uint32_t clkdiv = self->pio_slice->pio->sm[self->sm].clkdiv;
    return sysclk / (clkdiv / 256.0f);
}

void common_hal_rp2pio_sm_set_wrap(rp2pio_sm_obj_t *self, uint wrap_target, uint wrap) {
    uint loaded_offset = self->pio_slice->loaded_offset;
    sm_config_set_wrap(&self->config, loaded_offset + wrap_target, loaded_offset + wrap);
}

void common_hal_rp2pio_sm_set_shift(rp2pio_sm_obj_t *self, bool out, bool shift_right, bool _auto, uint threshold) {
    if (out) {
        sm_config_set_out_shift(&self->config, shift_right, _auto, threshold);
    } else {
        sm_config_set_in_shift(&self->config, shift_right, _auto, threshold);
    }
}

bool common_hal_rp2pio_sm_configure_fifo(rp2pio_sm_obj_t *self, uint ring_size_bits, bool tx, enum dma_channel_transfer_size transfer_size, bool bswap) {
    rp2pio_dmaringbuf_t *ringbuf = tx ? &self->tx_ringbuf : &self->rx_ringbuf;
    common_hal_rp2pio_sm_end_wait(self, tx);
    common_hal_rp2pio_dmaringbuf_deinit(ringbuf);

    PIO pio = self->pio_slice->pio;
    const volatile void *fifo_addr = tx ? &pio->txf[self->sm] : &pio->rxf[self->sm];
    if (!common_hal_rp2pio_dmaringbuf_alloc(ringbuf, ring_size_bits, pio_get_dreq(pio, self->sm, tx), transfer_size, bswap, (volatile void *)fifo_addr)) {
        common_hal_rp2pio_dmaringbuf_deinit(ringbuf);
        return false;
    }
    return true;
}

void common_hal_rp2pio_sm_reset(rp2pio_sm_obj_t *self, uint initial_pc) {
    common_hal_rp2pio_sm_end_wait(self, true);
    common_hal_rp2pio_dmaringbuf_clear(&self->tx_ringbuf);
    pio_sm_init(self->pio_slice->pio, self->sm, self->pio_slice->loaded_offset + initial_pc, &self->config);
    common_hal_rp2pio_sm_end_wait(self, false);
    common_hal_rp2pio_dmaringbuf_clear(&self->rx_ringbuf);
}

bool common_hal_rp2pio_sm_begin_wait(rp2pio_sm_obj_t *self, bool tx, rp2pio_pio_irq_handler_t handler, void *context) {
    rp2pio_dmaringbuf_t *ringbuf = tx ? &self->tx_ringbuf : &self->rx_ringbuf;
    bool *waiting = tx ? &self->tx_waiting : &self->rx_waiting;
    PIO pio = self->pio_slice->pio;
    enum pio_interrupt_source source = (tx ? pis_sm0_tx_fifo_not_full : pis_sm0_rx_fifo_not_empty) << self->sm;
    common_hal_rp2pio_pio_clear_irq(pio, source);

    if (!*waiting) {
        common_hal_rp2pio_dmaringbuf_set_enabled(ringbuf, false);
        size_t bufsize = common_hal_rp2pio_dmaringbuf_transfer(ringbuf, NULL, 1);
        if (bufsize) {
            common_hal_rp2pio_dmaringbuf_set_enabled(ringbuf, true);
        } else {
            *waiting = true;
        }
    }
    if (*waiting) {
        common_hal_rp2pio_pio_set_irq(pio, source, handler, context);
    }
    return *waiting;
}

void common_hal_rp2pio_sm_end_wait(rp2pio_sm_obj_t *self, bool tx) {
    rp2pio_dmaringbuf_t *ringbuf = tx ? &self->tx_ringbuf : &self->rx_ringbuf;
    bool *waiting = tx ? &self->tx_waiting : &self->rx_waiting;
    enum pio_interrupt_source source = (tx ? pis_sm0_tx_fifo_not_full : pis_sm0_rx_fifo_not_empty) << self->sm;
    common_hal_rp2pio_pio_clear_irq(self->pio_slice->pio, source);

    *waiting = false;
    if (ringbuf->channel != -1u) {
        common_hal_rp2pio_dmaringbuf_set_enabled(ringbuf, true);
    }
}

bool common_hal_rp2pio_sm_tx_from_source(enum pio_interrupt_source source, uint sm) {
    assert(source & ((pis_sm0_tx_fifo_not_full | pis_sm0_rx_fifo_not_empty) << sm));
    return source & (pis_sm0_tx_fifo_not_full << sm);
}

void common_hal_rp2pio_sm_debug(const mp_print_t *print, rp2pio_sm_obj_t *self) {
    PIO pio = self->pio_slice->pio;
    mp_printf(print, "sm %u on pio %u at %p\n", self->sm, pio_get_index(pio), self);
    mp_printf(print, "  clkdiv:    %08x\n", self->config.clkdiv);
    mp_printf(print, "  execctrl:  %08x\n", self->config.execctrl);
    mp_printf(print, "  shiftctrl: %08x\n", self->config.shiftctrl);
    mp_printf(print, "  pinctrl:   %08x\n", self->config.pinctrl);

    mp_printf(print, "  pc:        %u\n", pio_sm_get_pc(pio, self->sm));
    mp_printf(print, "  rx_fifo:   %u", pio_sm_get_rx_fifo_level(pio, self->sm));
    if (pio_sm_is_rx_fifo_full(pio, self->sm)) {
        mp_printf(print, " full");
    }
    mp_printf(print, "\n");
    mp_printf(print, "  tx_fifo:   %u", pio_sm_get_tx_fifo_level(pio, self->sm));
    if (pio_sm_is_tx_fifo_full(pio, self->sm)) {
        mp_printf(print, " full");
    }
    mp_printf(print, "\n");
}
