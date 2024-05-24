// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT
#pragma once

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#define TLS_INDEX_SYS 0
#define TLS_INDEX_APP 1

static_assert(TLS_INDEX_APP < configNUM_THREAD_LOCAL_STORAGE_POINTERS);

enum thread_interrupt_state {
    TASK_INTERRUPT_SET = 0x1,
    TASK_INTERRUPT_CAN_ABORT = 0x2,
};

typedef struct thread {
    struct thread *next;
    int ref_count;
    TaskHandle_t handle;
    UBaseType_t id;
    enum thread_interrupt_state state;

    TaskFunction_t entry;
    void *param;
    struct _reent *ptr;
    char *cwd;
    TaskHandle_t waiter;
    StaticTask_t buffer;
} thread_t;

// Thread creation
thread_t *thread_create(TaskFunction_t pxTaskCode, const char *pcName, const uint16_t usStackDepth, void *pvParameters, UBaseType_t uxPriority);

thread_t *thread_createStatic(TaskFunction_t pxTaskCode, const char *pcName, const uint16_t usStackDepth, void *pvParameters, UBaseType_t uxPriority, StackType_t *puxStackBuffer, StaticTask_t *pxTaskBuffer);


// Thread interruption
void thread_enable_interrupt();

void thread_disable_interrupt();

BaseType_t thread_interrupt(thread_t *thread);

int thread_check_interrupted();


// Thread join
int thread_join(thread_t *thread, TickType_t timeout);


// Thread reference management
inline thread_t *thread_current(void) {
    return pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_SYS);
}

// thread_t *thread_attach(thread_t *thread);

void thread_detach(thread_t *thread);

bool thread_iterate(thread_t **pthread);

thread_t *thread_lookup(UBaseType_t id);

TaskHandle_t thread_suspend(thread_t *thread);
void thread_resume(TaskHandle_t handle);

// Task utilities
inline StackType_t *task_pxTopOfStack(TaskHandle_t handle) {
    // assert(eTaskGetState(handle) == eSuspended);
    return *(StackType_t **)handle;
}
