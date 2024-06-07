/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
 * Copyright (c) 2023 Gregory Neverov
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

#include <malloc.h>
#include <sys/unistd.h>

#include "py/runtime.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "mpthreadport.h"


uint32_t mp_thread_begin_atomic_section(void) {
    taskENTER_CRITICAL();
    return 0;
}

void mp_thread_end_atomic_section(uint32_t state) {
    taskEXIT_CRITICAL();
}

#if MICROPY_PY_THREAD || 1
struct mp_thread_entry_shim {
    void *(*entry)(void *);
    void *arg;
};

static bool mp_thread_iterate(thread_t **pthread, mp_state_thread_t **pstate) {
    while (thread_iterate(pthread)) {
        thread_t *thread = *pthread;
        *pstate = pvTaskGetThreadLocalStoragePointer(thread->handle, TLS_INDEX_APP);
        if (*pstate) {
            return true;
        }
        thread_detach(thread);
    }
    *pstate = NULL;
    return false;
}

static void mp_thread_entry(void *pvParameters) {
    struct mp_thread_entry_shim *pshim = pvParameters;
    struct mp_thread_entry_shim shim = *pshim;
    free(pshim);
    shim.entry(shim.arg);
}

// Initialise threading support.
void mp_thread_init(void) {
}

// Shutdown threading support -- stops the second thread.
void mp_thread_deinit(void) {
    mp_obj_t exc = mp_obj_new_exception(&mp_type_SystemExit);
    thread_t *thread = NULL;
    mp_state_thread_t *state;
    while (mp_thread_iterate(&thread, &state)) {
        if (thread == thread_current()) {
            thread_detach(thread);
            continue;
        }
        state->mp_pending_exception = exc;
        thread_interrupt(thread);

        MP_THREAD_GIL_EXIT();
        thread_join(thread, portMAX_DELAY);
        MP_THREAD_GIL_ENTER();

        thread_detach(thread);
        thread = NULL;
    }
}

void mp_thread_gc_others(void) {
    thread_t *thread = NULL;
    mp_state_thread_t *state;
    while (mp_thread_iterate(&thread, &state)) {
        TaskHandle_t handle = thread_suspend(thread);
        if (handle != xTaskGetCurrentTaskHandle()) {
            void **stack_top = (void **)task_pxTopOfStack(handle);
            void **stack_bottom = (void **)state->stack_top;
            gc_collect_root(stack_top, stack_bottom - stack_top);
        }
        thread_resume(handle);
        thread_detach(thread);
    }
}

mp_uint_t mp_thread_get_id(void) {
    thread_t *thread = thread_current();
    return thread->id;
}

mp_uint_t mp_thread_create(void *(*entry)(void *), void *arg, size_t *stack_size) {
    if (*stack_size == 0) {
        *stack_size = 4096; // default stack size
    } else if (*stack_size < 2048) {
        *stack_size = 2048; // minimum stack size
    }

    // Round stack size to a multiple of the word size.
    size_t stack_num_words = *stack_size / sizeof(StackType_t);
    *stack_size = stack_num_words * sizeof(StackType_t);

    // Create thread on core1.
    struct mp_thread_entry_shim *shim = malloc(sizeof(struct mp_thread_entry_shim));
    shim->entry = entry;
    shim->arg = arg;
    thread_t *thread = thread_create(mp_thread_entry, "core1", stack_num_words, shim, 1);
    if (!thread) {
        free(shim);
        mp_raise_OSError(MP_ENOMEM);
    }

    // Adjust stack_size to provide room to recover from hitting the limit.
    *stack_size -= 512;
    UBaseType_t id = thread->id;
    thread_detach(thread);
    return id;
}

void mp_thread_start(void) {
}

void mp_thread_finish(void) {
    mp_thread_set_state(NULL);
}

void mp_thread_mutex_init(mp_thread_mutex_t *m) {
    m->handle = xSemaphoreCreateRecursiveMutexStatic(&m->buffer);
}

int mp_thread_mutex_lock(mp_thread_mutex_t *m, int wait) {
    return xSemaphoreTake(m->handle, wait ? portMAX_DELAY: 0);
}

void mp_thread_mutex_unlock(mp_thread_mutex_t *m) {
    xSemaphoreGive(m->handle);
}

#ifndef NDEBUG
bool mp_thread_mutex_check(mp_thread_mutex_t *m) {
    return xSemaphoreGetMutexHolder(m->handle) == xTaskGetCurrentTaskHandle();
}
#endif

#endif // MICROPY_PY_THREAD
