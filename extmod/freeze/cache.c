// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <memory.h>

#include "./freeze.h"
#include "py/mphal.h"


void freeze_cache_init(freeze_writer_t *freezer) {
    assert(freezer->num_flash_pages <= NUM_FLASH_PAGES);
    memset(freezer->cache_pages, 0, sizeof(freezer->cache_pages));
    memset(freezer->cache_ticks, 0, sizeof(freezer->cache_ticks));
}

static mp_flash_page_t *freeze_cache_put(freeze_writer_t *freezer, size_t page_num) {
    assert(page_num < freezer->num_flash_pages);
    mp_flash_page_t *cache_page = freezer->cache_pages[page_num];
    const mp_flash_page_t *flash_page = &freezer->flash_pages[page_num];
    assert(cache_page != NULL);
    mp_write_flash_page(flash_page, cache_page);
    return cache_page;
}

static mp_flash_page_t *freeze_cache_evict(freeze_writer_t *freezer) {
    size_t page_num = 0;
    while (freezer->cache_pages[page_num] == NULL) {
         if (page_num++ == freezer->num_flash_pages) {
            return NULL;
         }
    }
    size_t oldest_page_num = page_num++;
    while (page_num < freezer->num_flash_pages) {
        if ((freezer->cache_pages[page_num] != NULL) && (freezer->cache_ticks[page_num] < freezer->cache_ticks[oldest_page_num])) {
            oldest_page_num = page_num;
        }
        page_num++;
    }

    mp_flash_page_t *cached_page = freeze_cache_put(freezer, oldest_page_num);
    freezer->cache_pages[oldest_page_num] = NULL;
    return cached_page;
}

mp_flash_page_t *freeze_cache_get(freeze_writer_t *freezer, size_t page_num) {
    if (!freezer->cache_pages[page_num]) {
        mp_flash_page_t *cache_page = NULL;
        cache_page = malloc(sizeof(mp_flash_page_t));
        if (!cache_page) {
            cache_page = freeze_cache_evict(freezer);
        }
        if (!cache_page) {
            return NULL;
        }
        freezer->cache_pages[page_num] = cache_page;

        const mp_flash_page_t *flash_page = &freezer->flash_pages[page_num];
        mp_read_flash_page(cache_page, flash_page);
    }
    freezer->cache_ticks[page_num] = mp_hal_ticks_us();
    return freezer->cache_pages[page_num];
}

void freeze_cache_flush(freeze_writer_t *freezer, bool flush) {
    for (size_t page_num = 0; page_num < freezer->num_flash_pages; page_num++) {
        if (freezer->cache_pages[page_num]) {
            if (flush) {
                freeze_cache_put(freezer, page_num);
            }
            free(freezer->cache_pages[page_num]);
            freezer->cache_pages[page_num] = NULL;
        }
    }
}