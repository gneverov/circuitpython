// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>

#include "./extmod.h"


int mp_extmod_qstr(const uint16_t *qstr_table, size_t num_qstrs, uint16_t *qstr) {
    if (*qstr < MP_NUM_STATIC_QSTRS) {
        return 0;
    }
    else if (*qstr - MP_NUM_STATIC_QSTRS < num_qstrs) {
        *qstr = qstr_table[*qstr - MP_NUM_STATIC_QSTRS];
        return 0;
    }
    else {
        // printf("qstr index out of range\n");
        errno = EINVAL;
        return -1;
    }
}
