// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "FreeRTOS.h"
#include "task.h"

/* There are two implementations of flash lockout, which is needed to support writing to the XIP
 * flash. One uses interprocessor interrupts to stall the other processor, and the other uses a
 * high priority task to consume the other processor. The second implementation is similar to the
 * implementation of the Pico SDK's pico_flash library, but differs in that the lockout task is
 * created ahead of time. This is necessary as some consumers expect the lockout functions to be
 * non-allocating, and so they cannot allocate a task on-the-fly.
 */

#if configUSE_IPIS
#include "freertos/interrupts.h"

#include "hardware/irq.h"


static uint flash_lockout_irq_num;

static volatile uint flash_lockout_state;

void flash_lockout_start(void) {
    assert(flash_lockout_irq_num);

    vTaskSuspendAll();
    while (flash_lockout_state > 0) {
        if (!xTaskResumeAll()) {
            portYIELD();
        }
        vTaskSuspendAll();
    }
    ++flash_lockout_state;
    for (uint i = 0; i < configNUMBER_OF_CORES; i++) {
        if (i != portGET_CORE_ID()) {
            send_interprocessor_interrupt(i, flash_lockout_irq_num);
        }
    }
    vTaskPreemptionDisable(NULL);
    xTaskResumeAll();

    while (flash_lockout_state < configNUMBER_OF_CORES) {
        __wfe();
    }

    portDISABLE_INTERRUPTS();
}

void flash_lockout_end(void) {
    portENABLE_INTERRUPTS();

    assert(flash_lockout_state == configNUMBER_OF_CORES);
    flash_lockout_state = 0;
    __sev();
    vTaskPreemptionEnable(NULL);
}

static void __isr __not_in_flash_func(flash_lockout_handler)(void) {
    UBaseType_t ulState = portSET_INTERRUPT_MASK();
    ++flash_lockout_state;
    __sev();
    while (flash_lockout_state > 0) {
        __wfe();
    }
    portCLEAR_INTERRUPT_MASK(ulState);
}

__attribute__((visibility("hidden")))
void flash_lockout_init(void) {
    flash_lockout_irq_num = user_irq_claim_unused(true);
    irq_add_shared_handler(flash_lockout_irq_num, flash_lockout_handler, PICO_SHARED_IRQ_HANDLER_LOWEST_ORDER_PRIORITY);
    UBaseType_t uxCoreAffinityMask = vTaskCoreAffinityGet(NULL);
    for (uint i = 0; i < configNUMBER_OF_CORES; i++) {
        vTaskCoreAffinitySet(NULL, 1u << i);
        irq_set_enabled(flash_lockout_irq_num, true);
    }
    vTaskCoreAffinitySet(NULL, uxCoreAffinityMask);
}
#else
#include "semphr.h"


static SemaphoreHandle_t flash_lockout_mutex;
static TaskHandle_t flash_lockout_task;
static TaskHandle_t flash_lockout_holder;
static volatile uint flash_lockout_state;

static void __not_in_flash_func(flash_lockout_loop)(void *param) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        portDISABLE_INTERRUPTS();
        flash_lockout_state = 1;
        __sev();

        while (flash_lockout_state) {
            __wfe();
        }
        portENABLE_INTERRUPTS();

        xTaskNotifyGive(flash_lockout_holder);
        vTaskCoreAffinitySet(NULL, tskIDLE_PRIORITY);
    }
}

void flash_lockout_start(void) {
    assert(flash_lockout_task);
    xSemaphoreTakeRecursive(flash_lockout_mutex, portMAX_DELAY);

    xTaskNotifyStateClear(NULL);
    flash_lockout_holder = xTaskGetCurrentTaskHandle();
    xTaskNotifyGive(flash_lockout_task);
    vTaskCoreAffinitySet(flash_lockout_task, configMAX_PRIORITIES - 1);

    portDISABLE_INTERRUPTS();
    while (!flash_lockout_state) {
        __wfe();
    }
}

void flash_lockout_end(void) {
    assert(flash_lockout_state);
    flash_lockout_state = 0;
    __sev();
    portENABLE_INTERRUPTS();

    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    flash_lockout_task = NULL;
    xSemaphoreGiveRecursive(flash_lockout_mutex);
}

__attribute__((visibility("hidden")))
void flash_lockout_init(void) {
    static StaticSemaphore_t xMutexBuffer;
    flash_lockout_mutex = xSemaphoreCreateRecursiveMutexStatic(&xMutexBuffer);

    xTaskCreate(flash_lockout_loop, "flash lockout", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, &flash_lockout_task);
}
#endif
