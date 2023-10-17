// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "device/usbd_pvt.h"
#include "tinyusb/tusb_lock.h"


void tud_callback(tusb_cb_func_t func, void *arg) {
    usbd_defer_func(func, arg, false);
}

static SemaphoreHandle_t tud_mutex;
static StaticSemaphore_t tud_mutex_buffer;
static int tud_task_blocked;

void tud_lock_init(void) {
    tud_mutex = xSemaphoreCreateMutexStatic(&tud_mutex_buffer);
}

static void tud_sync(void *arg) {
    TaskHandle_t task = arg;
    tud_task_blocked = 1;
    xTaskNotifyGive(task);

    xSemaphoreTake(tud_mutex, portMAX_DELAY);
    tud_task_blocked = 0;
    xSemaphoreGive(tud_mutex);
}

void tud_lock(void) {
    xSemaphoreTake(tud_mutex, portMAX_DELAY);
    if (!tud_task_blocked) {
        xTaskNotifyStateClear(NULL);
        usbd_defer_func(tud_sync, xTaskGetCurrentTaskHandle(), false);
        do {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        }
        while (!tud_task_blocked);
    }
}

void tud_unlock(void) {
    xSemaphoreGive(tud_mutex);
}
