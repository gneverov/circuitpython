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

#if MICROPY_PY_THREAD
typedef struct _pythread_t {
    void *(*entry)(void *);
    void *arg;
    uint32_t usStackDepth;
    StackType_t *pxStackBase;
    TaskHandle_t task;
    struct _pythread_t *next;
} pythread_t;

MP_REGISTER_ROOT_POINTER(struct _pythread_t *thread_list);

STATIC void pythread_entry(void *pvParameters) {
    pythread_t *thread = pvParameters;
    task_init();
    void *arg = thread->arg;
    thread->arg = NULL;
    thread->entry(arg);
    vTaskDelete(NULL);
}

// Initialise threading support.
void mp_thread_init(void) {
}

// Shutdown threading support -- stops the second thread.
void mp_thread_deinit(void) {
}

void mp_thread_gc_others(void) {
    pythread_t *thread = MP_STATE_VM(thread_list);
    while (thread) {
        // Collect core1's stack if it is active.
        gc_collect_root((void **)thread->pxStackBase, thread->usStackDepth);
        gc_collect_root(&thread->arg, 1);
        thread = thread->next;
    }

    if (!mp_thread_is_main_thread()) {
        // GC running on core1, trace core0's stack.
        gc_collect_root((void **)&__MpStackBottom, (&__MpStackTop - &__MpStackBottom) / sizeof(uintptr_t));
    }
}

mp_uint_t mp_thread_get_id(void) {
    // On RP2, there are only two threads, one for each core, so the thread id
    // is the core number.
    return getpid();
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
    pythread_t *thread = m_new_obj(pythread_t);
    thread->entry = entry;
    thread->arg = arg;
    thread->usStackDepth = stack_num_words;
    thread->next = MP_STATE_VM(thread_list);
    if (xTaskCreate(pythread_entry, "core1", stack_num_words, thread, 1, &thread->task) != pdPASS) {
        mp_raise_OSError(MP_ENOMEM);
    }

    TaskStatus_t xTaskStatus;
    vTaskGetInfo(thread->task, &xTaskStatus, pdFALSE, eRunning);
    thread->pxStackBase = xTaskStatus.pxStackBase;

    // Adjust stack_size to provide room to recover from hitting the limit.
    *stack_size -= 512;

    MP_STATE_VM(thread_list) = thread;
    return xTaskStatus.xTaskNumber;
}

void mp_thread_start(void) {
}

void mp_thread_finish(void) {
    TaskHandle_t curr_task = xTaskGetCurrentTaskHandle();
    pythread_t **next = &MP_STATE_VM(thread_list);
    while (*next) {
        pythread_t *thread = *next;
        if (thread->task == curr_task) {
            *next = thread->next;
        } else {
            next = &thread->next;
        }
    }
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
