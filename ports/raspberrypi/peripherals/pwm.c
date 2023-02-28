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

#include "peripherals/pwm.h"

#include "src/rp2_common/hardware_pwm/include/hardware/pwm.h"

STATIC uint _claimed_mask;
STATIC uint _never_reset_mask;

void peripherals_pwm_reset(void) {
    uint reset_mask = _claimed_mask & ~_never_reset_mask;
    for (uint i = 0; i < NUM_PWM_SLICES; i++) {
        if (reset_mask & (1u << i)) {
            peripherals_pwm_unclaim(i);
        }
    }
    _claimed_mask &= _never_reset_mask;
}

bool peripherals_pwm_claim(uint pwm_slice) {
    uint bit = 1u << pwm_slice;
    if (_claimed_mask & bit) {
        return false;
    }
    _claimed_mask |= bit;
    return true;
}

void peripherals_pwm_never_reset(uint pwm_slice) {
    uint bit = 1u << pwm_slice;
    assert(_claimed_mask & bit);
    _never_reset_mask |= bit;
}

void peripherals_pwm_unclaim(uint pwm_slice) {
    uint bit = 1u << pwm_slice;
    assert(_claimed_mask & bit);

    if (_claimed_mask & bit) {
        pwm_set_enabled(pwm_slice, false);
    }
    _claimed_mask &= ~bit;
    _never_reset_mask &= ~bit;
}

void peripherals_pwm_debug(const mp_print_t *print, uint pwm_slice) {
    check_slice_num_param(pwm_slice);
    pwm_slice_hw_t hw = pwm_hw->slice[pwm_slice];
    mp_printf(print, "pwm slice %u\n", pwm_slice);
    mp_printf(print, "  en:          %d\n", (pwm_hw->en >> pwm_slice) & 1u);
    mp_printf(print, "  csr:         %08x\n", hw.csr);
    mp_printf(print, "  div:         %08x\n", hw.div);
    mp_printf(print, "  ctr:         %08x\n", hw.ctr);
    mp_printf(print, "  cc:          %08x\n", hw.cc);
    mp_printf(print, "  top:         %08x\n", hw.top);
}
