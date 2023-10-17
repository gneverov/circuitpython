// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/flash.h"

#include "py/mpconfig.h"


typedef uint8_t mp_flash_page_t[FLASH_SECTOR_SIZE];

bool mp_is_flash_ptr(const void *ptr);

bool mp_is_ram_ptr(const void *ptr);

void mp_write_flash_page(const mp_flash_page_t *flash_page, const mp_flash_page_t *ram_page);

void mp_read_flash_page(mp_flash_page_t *ram_page, const mp_flash_page_t *flash_page);