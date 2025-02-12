// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "morelib/flash_heap.h"
#include "py/runtime.h"

#define FREEZE_MODULE_FLASH_HEAP_TYPE 101
#define FREEZE_QSTR_POOL_FLASH_HEAP_TYPE 102

typedef struct {
    nlr_jump_callback_node_t nlr_callback;
    flash_heap_t heap;

    uint8_t *ram_start;
    uint8_t *ram_end;
    uint8_t *ram_pos;
    uint8_t *ram_limit;

    mp_map_t obj_map;
} freeze_writer_t;

void freeze_init();
void freeze_gc();

mp_obj_t freeze_clear(void);
mp_obj_t freeze_import_modules(size_t n_args, const mp_obj_t *args, mp_map_t *kwargs);
mp_obj_t freeze_get_modules(void);
