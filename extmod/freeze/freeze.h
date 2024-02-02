// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "./flash.h"
#include "py/runtime.h"


#define NUM_FLASH_PAGES 15
#define NUM_RAM_PAGES 1


typedef struct {
    nlr_jump_callback_node_t nlr_callback;

    const mp_flash_page_t *flash_pages;
    size_t num_flash_pages;
    size_t flash_size;
    size_t flash_pos;

    mp_flash_page_t *cache_pages[NUM_FLASH_PAGES];
    mp_uint_t cache_ticks[NUM_FLASH_PAGES];

    const mp_flash_page_t *ram_pages_in_flash;
    mp_flash_page_t *ram_pages;
    size_t num_ram_pages;
    size_t ram_size;
    uint8_t *ram_pos;

    bool ram_dirty[NUM_RAM_PAGES];

    const void *base;
    
    mp_map_t obj_map;
} freeze_writer_t;

void freeze_cache_init(freeze_writer_t *freezer);

mp_flash_page_t *freeze_cache_get(freeze_writer_t *freezer, size_t page_num);
 
void freeze_cache_flush(freeze_writer_t *freezer, bool flush);

bool freeze_clear();
void freeze_init();
void freeze_gc();

mp_obj_t freeze_import(size_t n_args, const mp_obj_t *args);
mp_obj_t freeze_modules(void);
