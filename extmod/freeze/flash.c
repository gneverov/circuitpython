// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <memory.h>

#include "./flash.h"


bool mp_is_flash_ptr(const void *ptr) {
    return (ptr == NULL) || ((ptr >= (void *)XIP_BASE) && (ptr < (void *)XIP_SRAM_END));
}

bool mp_is_ram_ptr(const void *ptr) {
    return (ptr == NULL) || ((ptr >= (void *)SRAM_BASE) && (ptr < (void *)SRAM_END));
}

void mp_write_flash_page(const mp_flash_page_t *flash_page, const mp_flash_page_t *ram_page) {
    assert((uintptr_t)flash_page >= XIP_BASE);
    assert(((uintptr_t)ram_page >= SRAM_BASE) && ((uintptr_t)ram_page < SRAM_END));
    uint32_t flash_offset = (uintptr_t)flash_page - XIP_BASE;

    mp_uint_t state = MICROPY_BEGIN_ATOMIC_SECTION();
    flash_range_erase(flash_offset, sizeof(mp_flash_page_t));
    flash_range_program(flash_offset, (const uint8_t *)ram_page, sizeof(mp_flash_page_t));
    MICROPY_END_ATOMIC_SECTION(state);
}

void mp_read_flash_page(mp_flash_page_t *ram_page, const mp_flash_page_t *flash_page) {
    assert((uintptr_t)flash_page >= XIP_BASE);
    assert(((uintptr_t)ram_page >= SRAM_BASE) && ((uintptr_t)ram_page < SRAM_END));
    memcpy(ram_page, flash_page, sizeof(mp_flash_page_t));
}