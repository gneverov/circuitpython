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

#include "common-hal/rp2pio/PioSlice.h"
#include "common-hal/rp2pio/Pio.h"
#include "common-hal/microcontroller/__init__.h"
#include "py/mperrno.h"

STATIC uint _used_sm_mask[NUM_PIOS];

STATIC uint *_get_used_sm_mask(PIO pio) {
    return &_used_sm_mask[pio_get_index(pio)];
}

void common_hal_rp2pio_pioslice_reset(void) {
    for (uint i = 0; i < NUM_PIOS; i++) {
        if (_used_sm_mask[i] == 0) {
            continue;
        }

        pio_set_sm_mask_enabled(all_pios[i], _used_sm_mask[i], false);
        for (uint j = 0; j < NUM_PIO_STATE_MACHINES; j++) {
            if (_used_sm_mask[i] & (1u << j)) {
                pio_sm_unclaim(all_pios[i], j);
            }
        }
        _used_sm_mask[i] = 0u;
    }
}

void common_hal_rp2pio_pioslice_init(rp2pio_pioslice_obj_t *self, const mp_obj_type_t *type, PIO pio, const pio_program_t *program) {
    self->base.type = type;
    self->pio = pio;
    self->program = *program;
    self->loaded_offset = -1;
    self->sm_mask = 0u;
    self->pin_mask = 0u;
}

void common_hal_rp2pio_pioslice_deinit(rp2pio_pioslice_obj_t *self) {
    for (uint sm = 0; sm < NUM_PIO_STATE_MACHINES; sm++) {
        common_hal_rp2pio_pioslice_release_sm(self, sm);
    }
    self->sm_mask = 0u;

    if (self->loaded_offset != -1u) {
        pio_remove_program(self->pio, &self->program, self->loaded_offset);
    }
    self->loaded_offset = -1;

    for (uint pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        common_hal_rp2pio_pioslice_release_pin(self, pin);
    }
    self->pin_mask = 0u;
}

bool common_hal_rp2pio_pioslice_claim(rp2pio_pioslice_obj_t *self, const mp_obj_type_t *type, const pio_program_t *program, uint num_sms, size_t num_pins, const mp_obj_t *pins) {
    for (uint i = 0; i < NUM_PIOS; i++) {
        common_hal_rp2pio_pioslice_init(self, type, all_pios[i], program);
        if (!pio_can_add_program(self->pio, &self->program)) {
            continue;
        }
        for (uint j = 0; j < num_sms; j++) {
            int sm = pio_claim_unused_sm(self->pio, false);
            if (sm == -1) {
                goto cleanup;
            }
            _used_sm_mask[i] |= 1u << (uint)sm;
            self->sm_mask |= 1u << (uint)sm;
        }
        for (uint j = 0; j < num_pins; j++) {
            mcu_pin_obj_t *pin = MP_OBJ_TO_PTR(pins[j]);
            if (!common_hal_rp2pio_pio_claim_pin(self->pio, pin)) {
                goto cleanup;
            }
            uint pin_num = common_hal_mcu_pin_number(pin);
            self->pin_mask |= 1u << pin_num;
        }
        self->loaded_offset = pio_add_program(self->pio, &self->program);
        return true;

    cleanup:
        common_hal_rp2pio_pioslice_deinit(self);
    }
    errno = MP_EBUSY;
    return false;
}

void common_hal_rp2pio_pioslice_release_sm(rp2pio_pioslice_obj_t *self, uint sm) {
    uint bit = 1u << sm;
    if (self->sm_mask & bit) {
        pio_sm_set_enabled(self->pio, sm, false);
        pio_sm_unclaim(self->pio, sm);
        *_get_used_sm_mask(self->pio) &= ~bit;
        self->sm_mask &= ~bit;
    }
}

void common_hal_rp2pio_pioslice_release_pin(rp2pio_pioslice_obj_t *self, uint pin) {
    uint bit = 1u << pin;
    if (self->pin_mask & bit) {
        const mcu_pin_obj_t *pin_obj = mcu_get_pin_by_number(pin);
        common_hal_rp2pio_pio_unclaim_pin(self->pio, pin_obj);
        self->pin_mask &= ~bit;
    }
}

// TODO: ability to claim pio irqs
