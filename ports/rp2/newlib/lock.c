// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <sys/lock.h>

#include "FreeRTOS.h"
#include "semphr.h"

struct __lock {
    SemaphoreHandle_t mutex;
    StaticSemaphore_t mutex_buffer;
};

struct __lock __lock___libc_recursive_mutex;

static void check_init(_LOCK_T lock) {
    assert(lock);
    if (!lock->mutex) {
        lock->mutex = xSemaphoreCreateMutexStatic(&lock->mutex_buffer);
    }
}

static void check_init_recursive(_LOCK_T lock) {
    assert(lock);
    if (!lock->mutex) {
        lock->mutex = xSemaphoreCreateRecursiveMutexStatic(&lock->mutex_buffer);
    }
}

void __retarget_lock_init(_LOCK_T *lock) {
    *lock = calloc(1, sizeof(struct __lock));
    check_init(*lock);
}

void __retarget_lock_init_recursive(_LOCK_T *lock) {
    *lock = calloc(1, sizeof(struct __lock));
    check_init_recursive(*lock);
}

void __retarget_lock_close(_LOCK_T lock) {
    assert(lock);
    vSemaphoreDelete(lock->mutex);
    free(lock);
}

void __retarget_lock_close_recursive(_LOCK_T lock) {
    assert(lock);
    vSemaphoreDelete(lock->mutex);
    free(lock);
}

void __retarget_lock_acquire(_LOCK_T lock) {
    check_init(lock);
    xSemaphoreTake(lock->mutex, portMAX_DELAY);
}

void __retarget_lock_acquire_recursive(_LOCK_T lock) {
    check_init_recursive(lock);
    xSemaphoreTakeRecursive(lock->mutex, portMAX_DELAY);
}

void __retarget_lock_release(_LOCK_T lock) {
    check_init(lock);
    xSemaphoreGive(lock->mutex);
}

void __retarget_lock_release_recursive(_LOCK_T lock) {
    check_init_recursive(lock);
    xSemaphoreGiveRecursive(lock->mutex);
}
