// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

#define FLASH_HEAP_NUM_DEVICES 2


typedef uintptr_t flash_ptr_t;

typedef struct {
    uint device;
    uint32_t type;
    int fd;
    flash_ptr_t flash_base;
    flash_ptr_t flash_start;
    flash_ptr_t flash_end;
    flash_ptr_t flash_limit;
    flash_ptr_t flash_pos;

    flash_ptr_t ram_start;
    flash_ptr_t ram_end;
    flash_ptr_t ram_limit;

    flash_ptr_t entry;
} flash_heap_t;

typedef struct {
    uint32_t type;
    uint32_t flash_size;
    uint32_t ram_size;
    void *ram_base;
    const void *entry;
} flash_heap_header_t;

const flash_heap_header_t *flash_heap_next_header(uint device);

int flash_heap_open(flash_heap_t *file, uint device, uint32_t type);

void flash_heap_free(flash_heap_t *file);

int flash_heap_close(flash_heap_t *file);

static inline flash_ptr_t flash_heap_tell(const flash_heap_t *file) {
    return file->flash_pos;
}

static inline const flash_heap_header_t *flash_heap_get_header(const flash_heap_t *file) {
    return (flash_heap_header_t *)file->flash_start;
}

int flash_heap_seek(flash_heap_t *file, flash_ptr_t pos);

int flash_heap_trim(flash_heap_t *file, flash_ptr_t pos);

flash_ptr_t flash_heap_align(flash_ptr_t addr, size_t align);

int flash_heap_write(flash_heap_t *file, const void *buffer, size_t length);

int flash_heap_read(flash_heap_t *file, void *buffer, size_t length);

int flash_heap_pwrite(flash_heap_t *file, const void *buffer, size_t length, flash_ptr_t pos);

int flash_heap_pread(flash_heap_t *file, void *buffer, size_t length, flash_ptr_t pos);

void *flash_heap_realloc_with_evict(flash_heap_t *file, void *ptr, size_t size);

bool flash_heap_is_valid_ptr(flash_heap_t *heap, flash_ptr_t pos);

bool flash_heap_iterate(uint device, const flash_heap_header_t **pheader);

int flash_heap_truncate(uint device, const flash_heap_header_t *header);

void flash_heap_stats(size_t *flash_size, size_t *ram_size);
