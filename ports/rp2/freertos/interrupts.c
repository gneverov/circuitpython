// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "freertos/interrupts.h"
#include "task.h"

#include "hardware/irq.h"


// Sets the core affinity of the executing task to the designated interrupt core.
UBaseType_t set_interrupt_core_affinity(void) {
    #if configNUMBER_OF_CORES > 1
    UBaseType_t uxCoreAffinityMask = vTaskCoreAffinityGet(NULL);
    vTaskCoreAffinitySet(NULL, INTERRUPT_CORE_AFFINITY_MASK);
    return uxCoreAffinityMask;
    #else
    return -1;
    #endif
}

// Restores the core affinity of the executing task after calling set_interrupt_core_affinity.
void clear_interrupt_core_affinity(UBaseType_t uxCoreAffinityMask) {
    #if configNUMBER_OF_CORES > 1
    vTaskCoreAffinitySet(NULL, uxCoreAffinityMask);
    #endif
}

// Returns true if the caller is executing on the designated interrupt core.
bool check_interrupt_core_affinity(void) {
    return (1u << portGET_CORE_ID()) & INTERRUPT_CORE_AFFINITY_MASK;
}

#if configUSE_IPIS
static uint64_t ipi_mask[configNUMBER_OF_CORES];

// Raises an interrupt on another core.
void send_interprocessor_interrupt(uint core_num, uint irq_num) {
    assert(core_num < configNUMBER_OF_CORES);
    taskENTER_CRITICAL();
    ipi_mask[core_num] |= 1ull << irq_num;
    portYIELD_CORE(core_num);
    taskEXIT_CRITICAL();
}

__attribute__((visibility("hidden")))
void vPortTaskSwitchHook(TaskHandle_t task) {
    // Called from vTaskSwitchContext, which is effectively a critical section since ISR_LOCK is held and interrupts are disabled.
    uint core_num = portGET_CORE_ID();
    uint64_t *irq_mask = &ipi_mask[core_num];
    while (*irq_mask) {
        uint irq_num = __builtin_ffsll(*irq_mask);
        irq_set_pending(irq_num - 1);
        *irq_mask >>= irq_num;
        *irq_mask <<= irq_num;
    }
}
#endif
