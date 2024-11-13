// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>
#include <poll.h>

#include "hardware/gpio.h"

#include "pico/fifo.h"
#include "pico/spi.h"

struct pico_spi_ll pico_spis_ll[NUM_SPIS] = {
    { .inst = spi0, 0 },
    { .inst = spi1, 0 },
};

__attribute__((constructor, visibility("hidden")))
void pico_spi_init(void) {
    for (int i = 0; i < NUM_SPIS; i++) {
        pico_spis_ll[i].mutex = xSemaphoreCreateMutexStatic(&pico_spis_ll[i].buffer);
    }
}

BaseType_t pico_spi_take(pico_spi_ll_t *spi, TickType_t xBlockTime) {
    TimeOut_t timeout;
    vTaskSetTimeOutState(&timeout);
    if (xSemaphoreTake(spi->mutex, xBlockTime) == pdFALSE) {
        return pdFALSE;
    }

    xTaskNotifyStateClear(NULL);
    for (;;) {
        BaseType_t timed_out = xTaskCheckForTimeOut(&timeout, &xBlockTime);
        taskENTER_CRITICAL();
        BaseType_t in_isr = spi->in_isr;
        spi->mutex_holder = in_isr && !timed_out ? xTaskGetCurrentTaskHandle() : NULL;
        taskEXIT_CRITICAL();
        if (!in_isr) {
            return pdTRUE;
        }
        if (timed_out) {
            return pdFALSE;
        }
        ulTaskNotifyTake(pdTRUE, xBlockTime);
    }
}

BaseType_t pico_spi_take_to_isr(pico_spi_ll_t *spi) {
    assert(xQueueGetMutexHolder(spi->mutex) == xTaskGetCurrentTaskHandle());
    assert(spi->mutex_holder == NULL);
    taskENTER_CRITICAL();
    spi->in_isr = 1;
    taskEXIT_CRITICAL();
    return xSemaphoreGive(spi->mutex);
}

void pico_spi_give_from_isr(pico_spi_ll_t *spi, BaseType_t *pxHigherPriorityTaskWoken) {
    assert(spi->in_isr);
    UBaseType_t state = taskENTER_CRITICAL_FROM_ISR();
    spi->in_isr = 0;
    TaskHandle_t task = spi->mutex_holder;
    taskEXIT_CRITICAL_FROM_ISR(state);
    if (task) {
        vTaskNotifyGiveFromISR(task, pxHigherPriorityTaskWoken);
    }
}

BaseType_t pico_spi_give(pico_spi_ll_t *spi) {
    assert(xQueueGetMutexHolder(spi->mutex) == xTaskGetCurrentTaskHandle());
    return xSemaphoreGive(spi->mutex);
}
