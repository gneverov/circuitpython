// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>

#include "freertos/task_helper.h"


enum task_interrupt_state {
    TASK_INTERRUPT_SET = 0x1,
    TASK_INTERRUPT_CAN_ABORT = 0x2,
};

void task_init() {
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_REENT, _REENT);
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_INTERRUPT, (void *)0);
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_APP, NULL);
}

void task_deinit() {
}

struct _reent *task_get_reent(TaskHandle_t task) {
    return pvTaskGetThreadLocalStoragePointer(task, TLS_INDEX_REENT);
}

void task_enable_interrupt() {
    vTaskSuspendAll();
    enum task_interrupt_state state = (intptr_t)pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_INTERRUPT);
    state |= TASK_INTERRUPT_CAN_ABORT;
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_INTERRUPT, (void *)state);
    xTaskResumeAll();
}

void task_disable_interrupt() {
    vTaskSuspendAll();
    enum task_interrupt_state state = (intptr_t)pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_INTERRUPT);
    state &= ~TASK_INTERRUPT_CAN_ABORT;
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_INTERRUPT, (void *)state);

    /* This code sets pxCurrentTCB->ucDelayAborted to pdFALSE. This flag is not useful for us
    because xTaskCheckForTimeOut does not distinguish between timeouts and interruptions. We use
    our own flag in TLS to record interruptions instead. */
    TimeOut_t xTimeOut;
    TickType_t xTicksToWait = portMAX_DELAY;
    xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait);
    xTaskResumeAll();
}

BaseType_t task_interrupt(TaskHandle_t task) {
    vTaskSuspendAll();
    enum task_interrupt_state state = (intptr_t)pvTaskGetThreadLocalStoragePointer(task, TLS_INDEX_INTERRUPT);
    state |= TASK_INTERRUPT_SET;
    vTaskSetThreadLocalStoragePointer(task, TLS_INDEX_INTERRUPT, (void *)state);
    xTaskResumeAll();

    if (state & TASK_INTERRUPT_CAN_ABORT) {
        return xTaskAbortDelay(task);
    }
    return pdFAIL;
}

int task_check_interrupted() {
    vTaskSuspendAll();
    enum task_interrupt_state state = (intptr_t)pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_INTERRUPT);
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_INTERRUPT, (void *)(state & ~TASK_INTERRUPT_SET));
    xTaskResumeAll();

    if (state & TASK_INTERRUPT_SET) {
        errno = EINTR;
        return -1;
    }
    return 0;
}
