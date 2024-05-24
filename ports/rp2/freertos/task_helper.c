// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>

#include "freertos/task_helper.h"

// Global mutex for thread operations
static SemaphoreHandle_t thread_mutex;

static thread_t *thread_list;

__attribute__((constructor))
void thread_init(void) {
    static StaticSemaphore_t buffer;
    thread_mutex = xSemaphoreCreateMutexStatic(&buffer);
}

static inline void thread_lock(void) {
    xSemaphoreTake(thread_mutex, portMAX_DELAY);
}

static inline void thread_unlock(void) {
    xSemaphoreGive(thread_mutex);
}

#ifndef NDEBUG
static inline bool thread_check_locked(void) {
    return xSemaphoreGetMutexHolder(thread_mutex) == xTaskGetCurrentTaskHandle();
}
#endif


// Thread creation
static void thread_entry(void *pvParameters) {
    thread_t *thread = pvParameters;
    thread->ptr = _REENT;
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_SYS, thread);
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_APP, NULL);

    // Wait for creation thread to release lock
    thread_lock();
    thread_unlock();

    thread->entry(thread->param);

    thread_lock();
    thread->handle = NULL;
    thread_t **pthread = &thread_list;
    while (*pthread) {
        if (*pthread == thread) {
            --thread->ref_count;
            if (thread->next) {
                thread->next->ref_count++;
            }
            *pthread = thread->next;
            break;
        }
        pthread = &thread->next;
    }

    thread->ptr = NULL;
    if (thread->waiter) {
        xTaskNotifyGive(thread->waiter);
        thread->waiter = NULL;
    }
    thread_unlock();
    thread_detach(thread);

    vTaskDelete(NULL);
}

static thread_t *thread_alloc(TaskFunction_t pxTaskCode, void *pvParameters) {
    thread_t *thread = malloc(sizeof(thread_t));
    if (thread) {
        thread->next = NULL;
        thread->ref_count = 1;
        thread->handle = NULL;
        thread->id = -1;
        thread->state = 0;
        thread->entry = pxTaskCode;
        thread->param = pvParameters;
        thread->ptr = NULL;
        thread->cwd = NULL;
        thread->waiter = NULL;
    }
    return thread;
}

static void thread_initialize(thread_t *thread, TaskHandle_t handle) {
    thread->handle = handle;

    // Increment ref count for copy passed as thread entry parameter
    thread->ref_count++;

    // Get the new thread's ID
    TaskStatus_t thread_status;
    vTaskGetInfo(thread->handle, &thread_status, pdFALSE, eRunning);
    thread->id = thread_status.xTaskNumber;

    // Insert new thread in list of all threads
    thread->next = thread_list;
    thread->ref_count++;
    thread_list = thread;
}

thread_t *thread_create(TaskFunction_t pxTaskCode, const char *pcName, const uint16_t usStackDepth, void *pvParameters, UBaseType_t uxPriority) {
    thread_t *thread = thread_alloc(pxTaskCode, pvParameters);
    if (!thread) {
        return NULL;
    }
    thread_lock();
    TaskHandle_t handle;
    if (xTaskCreate(thread_entry, pcName, usStackDepth, thread, uxPriority, &handle) == pdPASS) {
        thread_initialize(thread, handle);
        thread_unlock();
    } else {
        thread_unlock();
        thread_detach(thread);
        thread = NULL;
    }
    return thread;
}

thread_t *thread_createStatic(TaskFunction_t pxTaskCode, const char *pcName, const uint16_t usStackDepth, void *pvParameters, UBaseType_t uxPriority, StackType_t *puxStackBuffer, StaticTask_t *pxTaskBuffer) {
    thread_t *thread = thread_alloc(pxTaskCode, pvParameters);
    if (!thread) {
        return NULL;
    }
    thread_lock();
    TaskHandle_t handle = xTaskCreateStatic(thread_entry, pcName, usStackDepth, thread, uxPriority, puxStackBuffer, pxTaskBuffer);
    if (handle) {
        thread_initialize(thread, handle);
        thread_unlock();
    } else {
        thread_unlock();
        thread_detach(thread);
        thread = NULL;
    }
    return thread;
}


void thread_enable_interrupt(void) {
    thread_t *thread = thread_current();
    thread_lock();
    thread->state |= TASK_INTERRUPT_CAN_ABORT;
    thread_unlock();
}

void thread_disable_interrupt(void) {
    thread_t *thread = thread_current();
    thread_lock();
    thread->state &= ~TASK_INTERRUPT_CAN_ABORT;

    /* This code sets pxCurrentTCB->ucDelayAborted to pdFALSE. This flag is not useful for us
    because xTaskCheckForTimeOut does not distinguish between timeouts and interruptions. We use
    our own flag in TLS to record interruptions instead. */
    TimeOut_t xTimeOut;
    TickType_t xTicksToWait = portMAX_DELAY;
    xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait);
    thread_unlock();
}

BaseType_t thread_interrupt(thread_t *thread) {
    BaseType_t ret = pdFAIL;
    thread_lock();
    thread->state |= TASK_INTERRUPT_SET;
    if ((thread->state & TASK_INTERRUPT_CAN_ABORT) && thread->handle) {
        ret = xTaskAbortDelay(thread->handle);
    }
    thread_unlock();
    return ret;
}

int thread_check_interrupted(void) {
    thread_t *thread = thread_current();
    thread_lock();
    enum thread_interrupt_state state = thread->state;
    thread->state &= ~TASK_INTERRUPT_SET;
    thread_unlock();

    if (state & TASK_INTERRUPT_SET) {
        errno = EINTR;
        return -1;
    }
    return 0;
}


// void thread_attach(thread_t *thread) {
//     assert(thread_check_locked());
//     if (!thread) {
//         return;
//     }
//     assert(thread->ref_count > 0);
//     thread->ref_count++;
// }

void thread_detach(thread_t *thread) {
    // assert(thread_check_locked());
    if (!thread) {
        return;
    }
    thread_lock();
    int ref_count = --thread->ref_count;
    thread_unlock();
    if (ref_count == 0) {
        assert(thread->handle == NULL);
        thread_detach(thread->next);
        free(thread->cwd);
        free(thread);
    }
}


int thread_join(thread_t *thread, TickType_t timeout) {
    if (thread->handle == xTaskGetCurrentTaskHandle()) {
        errno = EINVAL;
        return -1;
    }
    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    while (!xTaskCheckForTimeOut(&xTimeOut, &timeout)) {
        if (thread_check_interrupted()) {
            return -1;
        }

        thread_lock();
        TaskHandle_t handle = thread->handle;
        if (handle) {
            // assert(!thread->waiter);
            thread->waiter = xTaskGetCurrentTaskHandle();
        }
        thread_unlock();
        if (!handle) {
            return 0;
        }

        thread_enable_interrupt();
        ulTaskNotifyTake(pdTRUE, timeout);
        thread_disable_interrupt();
    }
    errno = EAGAIN;
    return -1;
}

bool thread_iterate(thread_t **pthread) {
    thread_t *thread = *pthread;
    thread_lock();
    if (thread) {
        thread = thread->next;
    } else {
        thread = thread_list;
    }
    while (thread && !thread->handle) {
        thread = thread->next;
    }
    if (thread) {
        thread->ref_count++;
    }
    thread_unlock();
    *pthread = thread;
    return thread;
}

thread_t *thread_lookup(UBaseType_t id) {
    thread_t *ret = NULL;
    thread_t *thread = NULL;
    while (thread_iterate(&thread)) {
        if (thread->id == id) {
            ret = thread;
            break;
        }
        thread_detach(thread);
    }
    return ret;
}

TaskHandle_t thread_suspend(thread_t *thread) {
    thread_lock();
    TaskHandle_t handle = thread->handle;
    if (handle && (handle != xTaskGetCurrentTaskHandle())) {
        vTaskSuspend(handle);
    }
    thread_unlock();
    return handle;
}

void thread_resume(TaskHandle_t handle) {
    if (handle != xTaskGetCurrentTaskHandle()) {
        vTaskResume(handle);
    }
}
