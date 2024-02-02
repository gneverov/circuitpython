// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>

#include "py/mpthread.h"


typedef struct gc_handle gc_handle_t;

gc_handle_t *gc_handle_alloc(void *gc_ptr);

gc_handle_t *gc_handle_copy(gc_handle_t *gc_handle);

void *gc_handle_get(const gc_handle_t *gc_handle);

void gc_handle_free(gc_handle_t *gc_handle);

void gc_handle_collect(bool clear);

#ifndef NDEBUG
#define gc_handle_check() assert(MP_THREAD_GIL_CHECK())
#else
#define gc_handle_check()
#endif
