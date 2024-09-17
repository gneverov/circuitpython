// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <sys/types.h>

#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE (1u << 12)
#endif

#ifndef FLASH_BASE
#define FLASH_BASE 0x10000000u
#endif

#ifndef PSRAM_BASE
#define PSRAM_BASE 0u
#endif

extern size_t flash_size;
extern size_t psram_size;
extern size_t flash_storage_offset;
extern size_t flash_storage_size;

void flash_memread(uint32_t flash_offs, void *mem, size_t size);

void flash_memwrite(uint32_t flash_offs, const void *mem, size_t size);
