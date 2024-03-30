// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/obj.h"

#define MP_NUM_STATIC_QSTRS (MP_QSTR_zip + 1)

typedef struct mp_extension_module {
    size_t num_qstrs;
    const uint16_t *qstr_table;
    const char *const *qstrs;
    const mp_rom_obj_t *object_start;
    const mp_rom_obj_t *object_end;
} mp_extension_module_t;

int mp_extmod_qstr(const uint16_t *qstr_table, size_t num_qstrs, uint16_t *qstr);
