// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "py/gc_handle.h"
#include "py/gc.h"

#if MICROPY_FREERTOS

struct gc_handle {
    void *gc_ptr;
    int ref_count;
    struct gc_handle *next;
};

static gc_handle_t *gc_handle_list;

gc_handle_t *gc_handle_alloc(void *gc_ptr) {
    gc_handle_check();
    gc_handle_t **next = &gc_handle_list;
    while (*next) {
        gc_handle_t *gc_handle = *next;
        if (gc_handle->gc_ptr == gc_ptr) {
            return gc_handle_copy(gc_handle);
        }
        next = &gc_handle->next;
    }

    gc_handle_t *gc_handle = malloc(sizeof(gc_handle_t));
    gc_handle->gc_ptr = gc_ptr;
    gc_handle->ref_count = 1;
    gc_handle->next = gc_handle_list;
    gc_handle_list = gc_handle;
    return gc_handle;
}

void *gc_handle_get(const gc_handle_t *gc_handle) {
    gc_handle_check();
    assert(gc_handle->ref_count > 0);
    return gc_handle->gc_ptr;
}

gc_handle_t *gc_handle_copy(gc_handle_t *gc_handle) {
    assert(gc_handle->ref_count >= 0);
    uint32_t state = MICROPY_BEGIN_ATOMIC_SECTION();
    gc_handle->ref_count++;
    MICROPY_END_ATOMIC_SECTION(state);
    return gc_handle;
}

void gc_handle_free(gc_handle_t *gc_handle) {
    assert(gc_handle->ref_count > 0);
    uint32_t state = MICROPY_BEGIN_ATOMIC_SECTION();
    gc_handle->ref_count--;
    MICROPY_END_ATOMIC_SECTION(state);
}

void gc_handle_collect(bool clear) {
    gc_handle_t **next = &gc_handle_list;
    while (*next) {
        gc_handle_t *gc_handle = *next;
        if (gc_handle->ref_count <= 0) {
            *next = gc_handle->next;
            free(gc_handle);
        } else {
            if (!clear && gc_handle->gc_ptr) {
                gc_collect_root(&gc_handle->gc_ptr, 1);
            } else {
                gc_handle->gc_ptr = NULL;
            }
            next = &gc_handle->next;
        }
    }
}
#endif
