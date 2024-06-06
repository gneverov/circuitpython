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

typedef struct mp_obj_qstr_array {
    mp_obj_base_t base;
    const void *array;
    size_t array_size;
    uint16_t elem_size;
    uint16_t qstr_offset;
} mp_obj_qstr_array_t;

int mp_extmod_qstr(const uint16_t *qstr_table, size_t num_qstrs, uint16_t *qstr);

extern const mp_obj_type_t mp_type_qstr_array;

#if MICROPY_PY_EXTENSION
#define MP_REGISTER_STRUCT(var, type) \
STATIC const mp_obj_qstr_array_t var##_link = { \
    .base = { &mp_type_qstr_array }, \
    .array = &var, \
    .array_size = sizeof(var), \
    .elem_size = sizeof(type), \
    .qstr_offset = 0, \
}; \
MP_REGISTER_OBJECT(var##_link)
#else
#define MP_REGISTER_STRUCT(var, type)
#endif
