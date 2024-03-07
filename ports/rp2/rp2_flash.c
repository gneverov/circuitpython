/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "mpconfigport.h"
#include "hardware/flash.h"
#include "pico/binary_info.h"

#define BLOCK_SIZE_BYTES (FLASH_SECTOR_SIZE)

#ifndef MICROPY_HW_FLASH_STORAGE_BYTES
#define MICROPY_HW_FLASH_STORAGE_BYTES (1024 * 1024)
#endif
static_assert(MICROPY_HW_FLASH_STORAGE_BYTES % 4096 == 0, "Flash storage size must be a multiple of 4K");

#ifndef MICROPY_HW_FLASH_STORAGE_BASE
#define MICROPY_HW_FLASH_STORAGE_BASE (PICO_FLASH_SIZE_BYTES - MICROPY_HW_FLASH_STORAGE_BYTES)
#endif

static_assert(MICROPY_HW_FLASH_STORAGE_BYTES <= PICO_FLASH_SIZE_BYTES, "MICROPY_HW_FLASH_STORAGE_BYTES too big");
static_assert(MICROPY_HW_FLASH_STORAGE_BASE + MICROPY_HW_FLASH_STORAGE_BYTES <= PICO_FLASH_SIZE_BYTES, "MICROPY_HW_FLASH_STORAGE_BYTES too big");

// Tag the flash drive in the binary as readable/writable (but not reformatable)
bi_decl(bi_block_device(
    BINARY_INFO_TAG_MICROPYTHON,
    "MicroPython",
    XIP_BASE + MICROPY_HW_FLASH_STORAGE_BASE,
    MICROPY_HW_FLASH_STORAGE_BYTES,
    NULL,
    BINARY_INFO_BLOCK_DEV_FLAG_READ |
    BINARY_INFO_BLOCK_DEV_FLAG_WRITE |
    BINARY_INFO_BLOCK_DEV_FLAG_PT_UNKNOWN));
