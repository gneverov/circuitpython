// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <sys/unistd.h>

#include "FreeRTOS.h"
#include "task.h"

#include "newlib/flash_heap.h"
#include "newlib/dlfcn.h"

#define FLASH_HEAP_NUM_PAGES 16


extern uint8_t __flash_heap_end;
extern uint8_t __StackLimit;
extern uint8_t end;

__attribute__((section(".flash_heap")))
static flash_heap_header_t flash_heap_head = { 0, 0, 0, &end, NULL };

static const flash_heap_header_t *flash_heap_tail;

__attribute__((constructor))
void flash_heap_init(void) {
    const flash_heap_header_t *header = &flash_heap_head;
    while (header->type) {
        void *ram_base = sbrk(header->ram_size);
        if (header->ram_base != ram_base) {
            panic("flash heap corrupt");
            return;
        }
        header = (flash_heap_header_t *)((uint8_t *)header + header->flash_size);
    }
    flash_heap_tail = header;

    dl_init(DT_INIT);
}

const flash_heap_header_t *flash_heap_next_header(void) {
    return flash_heap_tail;
}

static void flash_heap_read_page(flash_page_t *ram_page, const flash_page_t *flash_page) {
    assert(((uintptr_t)flash_page >= (uintptr_t)&flash_heap_head) && ((uintptr_t)flash_page < (uintptr_t)&__flash_heap_end));
    assert(((uintptr_t)ram_page >= (uintptr_t)&end) && ((uintptr_t)ram_page < (uintptr_t)&__StackLimit));
    memcpy(ram_page, flash_page, sizeof(flash_page_t));
}

static void flash_heap_write_page(const flash_page_t *flash_page, const flash_page_t *ram_page) {
    assert(((uintptr_t)flash_page >= (uintptr_t)&flash_heap_head) && ((uintptr_t)flash_page < (uintptr_t)&__flash_heap_end));
    assert(((uintptr_t)ram_page >= (uintptr_t)&end) && ((uintptr_t)ram_page < (uintptr_t)&__StackLimit));
    uint32_t flash_offset = (uintptr_t)flash_page - XIP_BASE;

    taskENTER_CRITICAL();
    flash_range_erase(flash_offset, sizeof(flash_page_t));
    flash_range_program(flash_offset, (const uint8_t *)ram_page, sizeof(flash_page_t));
    taskEXIT_CRITICAL();

    assert(memcmp(flash_page, ram_page, sizeof(flash_page_t)) == 0);
}

// inline size_t flash_heap_num_flash_pages(flash_heap_t *file) {
//     return (file->flash_end - file->flash_start + sizeof(flash_page_t) - 1) / sizeof(flash_page_t);
// }

static flash_page_t *flash_heap_put_page(flash_heap_t *file, size_t page_num) {
    assert(page_num < file->num_cache_pages);
    flash_page_t *cache_page = file->cache_pages[page_num];
    const flash_page_t *flash_page = &file->flash_pages[page_num];
    assert(cache_page != NULL);
    flash_heap_write_page(flash_page, cache_page);
    return cache_page;
}

static flash_page_t *flash_heap_evict_page(flash_heap_t *file) {
    size_t page_num = 0;
    while (file->cache_pages[page_num] == NULL) {
        if (++page_num == file->num_cache_pages) {
            return NULL;
        }
    }
    size_t oldest_page_num = page_num++;
    while (page_num < file->num_cache_pages) {
        if ((file->cache_pages[page_num] != NULL) && (file->cache_ticks[page_num] < file->cache_ticks[oldest_page_num])) {
            oldest_page_num = page_num;
        }
        page_num++;
    }

    flash_page_t *cached_page = flash_heap_put_page(file, oldest_page_num);
    file->cache_pages[oldest_page_num] = NULL;
    return cached_page;
}

void *flash_heap_realloc_with_evict(flash_heap_t *file, void *ptr, size_t size) {
    void *new_ptr = realloc(ptr, size);
    while (!new_ptr) {
        flash_page_t *evicted_page = flash_heap_evict_page(file);
        if (!evicted_page) {
            errno = ENOMEM;
            return NULL;
        }
        free(evicted_page);
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wuse-after-free"
        new_ptr = realloc(ptr, size);
        #pragma GCC diagnostic pop
    }
    return new_ptr;
}

static int flash_heap_ensure_cache_pages(flash_heap_t *file, size_t page_num) {
    while (page_num >= file->num_cache_pages) {
        flash_page_t **new_cache_pages = flash_heap_realloc_with_evict(file, file->cache_pages, sizeof(flash_page_t *) * file->num_cache_pages * 2);
        if (!new_cache_pages) {
            return -1;
        }
        file->cache_pages = new_cache_pages;
        memset(file->cache_pages + file->num_cache_pages, 0, sizeof(flash_page_t *) * file->num_cache_pages);

        uint *new_cache_ticks = flash_heap_realloc_with_evict(file, file->cache_ticks, sizeof(uint) * file->num_cache_pages * 2);
        if (!new_cache_ticks) {
            return -1;
        }
        file->cache_ticks = new_cache_ticks;
        memset(file->cache_ticks + file->num_cache_pages, 0, sizeof(uint) * file->num_cache_pages);

        file->num_cache_pages *= 2;
    }
    return 0;
}

static flash_page_t *flash_heap_get_page(flash_heap_t *file, size_t page_num) {
    if (flash_heap_ensure_cache_pages(file, page_num) < 0) {
        return NULL;
    }
    if (!file->cache_pages[page_num]) {
        flash_page_t *cache_page = NULL;
        cache_page = malloc(sizeof(flash_page_t));
        if (!cache_page) {
            cache_page = flash_heap_evict_page(file);
        }
        if (!cache_page) {
            errno = ENOMEM;
            return NULL;
        }
        file->cache_pages[page_num] = cache_page;

        const flash_page_t *flash_page = &file->flash_pages[page_num];
        flash_heap_read_page(cache_page, flash_page);
    }
    file->cache_ticks[page_num] = ++file->next_tick;
    return file->cache_pages[page_num];
}

static void *flash_heap_get(flash_heap_t *file, flash_ptr_t ptr, size_t *len) {
    switch (ptr >> 28) {
        case 1: {
            if (ptr >= file->flash_limit) {
                errno = ENOSPC;
                return NULL;
            }
            size_t max_len = file->flash_limit - ptr;
            size_t page_num = (ptr - (flash_ptr_t)file->flash_pages) / sizeof(flash_page_t);
            flash_page_t *page = flash_heap_get_page(file, page_num);
            if (page == NULL) {
                return NULL;
            }
            size_t offset = (uintptr_t)ptr % sizeof(flash_page_t);
            *len = MIN(sizeof(flash_page_t) - offset, max_len);
            return *page + offset;
        }
        default: {
            errno = EFAULT;
            return NULL;
        }
    }
}

void flash_heap_free(flash_heap_t *file) {
    for (size_t page_num = 0; page_num < file->num_cache_pages; page_num++) {
        free(file->cache_pages[page_num]);
        file->cache_pages[page_num] = NULL;
    }
    free(file->cache_pages);
    file->cache_pages = NULL;
    free(file->cache_ticks);
    file->cache_ticks = NULL;
    file->num_cache_pages = 0;
}

int flash_heap_open(flash_heap_t *file, uint32_t type) {
    memset(file, 0, sizeof(flash_heap_t));
    file->type = type;

    file->flash_pages = (flash_page_t *)((uintptr_t)flash_heap_tail & ~(sizeof(flash_page_t) - 1));
    file->flash_start = (flash_ptr_t)flash_heap_tail;
    file->flash_end = file->flash_start + sizeof(flash_heap_header_t);
    file->flash_limit = (flash_ptr_t)&__flash_heap_end;
    file->flash_pos = file->flash_end;

    file->ram_start = (flash_ptr_t)flash_heap_tail->ram_base;
    file->ram_end = file->ram_start;
    file->ram_limit = (flash_ptr_t)&__StackLimit;

    file->num_cache_pages = FLASH_HEAP_NUM_PAGES;
    file->cache_pages = calloc(file->num_cache_pages, sizeof(flash_page_t *));
    file->cache_ticks = calloc(file->num_cache_pages, sizeof(uint));

    if (!file->cache_pages || !file->cache_ticks) {
        flash_heap_free(file);
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

int flash_heap_close(flash_heap_t *file) {
    int ret = -1;
    size_t ram_size = file->ram_end - file->ram_start;
    flash_ptr_t new_tail = flash_heap_align(file->flash_end, __alignof__(flash_heap_header_t));
    flash_heap_header_t header;
    header.type = file->type;
    header.flash_size = new_tail - file->flash_start;
    header.ram_size = ram_size;
    header.ram_base = flash_heap_tail->ram_base;
    header.entry = (void *)file->entry;
    if (flash_heap_pwrite(file, &header, sizeof(header), file->flash_start) < 0) {
        goto cleanup;
    }

    header.type = 0;
    header.flash_size = 0;
    header.ram_size = 0;
    header.ram_base = flash_heap_tail->ram_base + ram_size;
    header.entry = NULL;
    if (flash_heap_pwrite(file, &header, sizeof(header), new_tail) < 0) {
        goto cleanup;
    }

    for (size_t page_num = 0; page_num < file->num_cache_pages; page_num++) {
        if (file->cache_pages[page_num]) {
            flash_heap_put_page(file, page_num);
        }
    }
    flash_heap_tail = (void *)new_tail;
    ret = 0;

cleanup:
    flash_heap_free(file);
    return ret;
}

int flash_heap_seek(flash_heap_t *file, flash_ptr_t pos) {
    flash_ptr_t start;
    flash_ptr_t *end;
    flash_ptr_t limit;
    flash_ptr_t *ppos;
    switch (pos >> 28) {
        case 1:
            start = file->flash_start;
            end = &file->flash_end;
            limit = file->flash_limit;
            ppos = &file->flash_pos;
            break;
        case 2:
            start = file->ram_start;
            end = &file->ram_end;
            limit = file->ram_limit;
            ppos = NULL;
            break;
        default:
            errno = EFAULT;
            return -1;
    }
    if (pos < start) {
        errno = EINVAL;
        return -1;
    }
    if (pos >= limit) {
        errno = ENOSPC;
        return -1;
    }
    *end = MAX(*end, pos);
    if (ppos) {
        *ppos = pos;
    }
    return 0;
}

int flash_heap_trim(flash_heap_t *file, flash_ptr_t pos) {
    if (flash_heap_seek(file, pos) < 0) {
        return -1;
    }
    switch (pos >> 28) {
        case 1: {
            file->flash_end = pos;
            size_t page_num = (file->flash_pos - (flash_ptr_t)file->flash_pages + sizeof(flash_page_t) - 1) / sizeof(flash_page_t);
            while (page_num < file->num_cache_pages) {
                free(file->cache_pages[page_num]);
                file->cache_pages[page_num] = NULL;
                page_num++;
            }
            break;
        }
        case 2: {
            file->ram_end = pos;
            break;
        }
        default: {
            errno = EFAULT;
            return -1;
        }
    }
    return 0;
}

flash_ptr_t flash_heap_align(flash_ptr_t addr, size_t align) {
    assert((align & (align - 1)) == 0);
    return (addr + align - 1) & ~(align - 1);
}

int flash_heap_write(flash_heap_t *file, const void *buffer, size_t size) {
    size_t remaining = size;
    while (remaining > 0) {
        size_t len;
        uint8_t *out = flash_heap_get(file, file->flash_pos, &len);
        if (!out) {
            return -1;
        }
        len = MIN(len, remaining);
        memcpy(out, buffer, len);
        buffer += len;
        remaining -= len;
        file->flash_pos += len;
        file->flash_end = MAX(file->flash_end, file->flash_pos);
    }
    return size;
}

int flash_heap_read(flash_heap_t *file, void *buffer, size_t size) {
    size = MIN(size, file->flash_end - file->flash_pos);
    size_t remaining = size;
    while (remaining > 0) {
        size_t len;
        uint8_t *out = flash_heap_get(file, file->flash_pos, &len);
        if (!out) {
            return -1;
        }
        len = MIN(len, remaining);
        memcpy(buffer, out, len);
        buffer += len;
        remaining -= len;
        file->flash_pos += len;
    }
    return size;
}

int flash_heap_pwrite(flash_heap_t *file, const void *buffer, size_t length, flash_ptr_t pos) {
    if (flash_heap_seek(file, pos) < 0) {
        return -1;
    }
    return flash_heap_write(file, buffer, length);
}

int flash_heap_pread(flash_heap_t *file, void *buffer, size_t length, flash_ptr_t pos) {
    if (flash_heap_seek(file, pos) < 0) {
        return -1;
    }
    return flash_heap_read(file, buffer, length);
}

bool flash_heap_is_valid_ptr(flash_heap_t *heap, flash_ptr_t pos) {
    return
        ((pos >= heap->flash_start) && (pos <= heap->flash_end)) ||
        ((pos >= heap->ram_start) && (pos <= heap->ram_end));
}

bool flash_heap_iterate(const flash_heap_header_t **pheader) {
    const flash_heap_header_t *header = *pheader;
    if (header == NULL) {
        *pheader = &flash_heap_head;
    } else {
        *pheader = (flash_heap_header_t *)(((uint8_t *)header) + header->flash_size);
    }
    return (*pheader)->type;
}

int flash_heap_truncate(const flash_heap_header_t *header) {
    if (!header) {
        header = (flash_heap_header_t *)((void *)&flash_heap_head + flash_heap_head.flash_size);
    }
    flash_page_t *ram_page = malloc(sizeof(*ram_page));
    if (!ram_page) {
        errno = ENOMEM;
        return -1;
    }

    flash_ptr_t fptr = (flash_ptr_t)header;
    size_t foffset = fptr % sizeof(flash_page_t);
    const flash_page_t *flash_page = (void *)(fptr - foffset);
    flash_heap_read_page(ram_page, flash_page);
    flash_heap_header_t *ptr = (void *)ram_page + foffset;
    ptr->type = 0;
    ptr->flash_size = 0;
    ptr->ram_size = 0;
    ptr->entry = NULL;

    flash_heap_write_page(flash_page, ram_page);
    flash_heap_tail = header;
    free(ram_page);
    return 0;
}

void flash_heap_stats(size_t *flash_size, size_t *ram_size) {
    *flash_size = (uintptr_t)flash_heap_tail - XIP_BASE;
    *ram_size = (uintptr_t)flash_heap_tail->ram_base - SRAM_BASE;
}
