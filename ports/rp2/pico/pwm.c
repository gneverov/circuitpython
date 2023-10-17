// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "hardware/pwm.h"

#include "pico/pwm.h"


#ifndef NDEBUG
#include <stdio.h>

void pico_pwm_debug(uint slice) {
    check_slice_num_param(slice);
    pwm_slice_hw_t *hw = &pwm_hw->slice[slice];
    printf("pwm slice %u\n", slice);
    printf("  en:          %ld\n", (pwm_hw->en >> slice) & 1u);
    printf("  csr:         0x%08lx\n", hw->csr);
    printf("  div:         0x%08lx\n", hw->div);
    printf("  ctr:         0x%08lx\n", hw->ctr);
    printf("  cc:          0x%08lx\n", hw->cc);
    printf("  top:         0x%08lx\n", hw->top);
}
#endif
