// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "freertos/interrupts.h"
#include "task.h"

#include "hardware/irq.h"


// Sets the core affinity of the executing task to the designated interrupt core.
UBaseType_t set_interrupt_core_affinity(void) {
    UBaseType_t uxCoreAffinityMask = vTaskCoreAffinityGet(NULL);
    vTaskCoreAffinitySet(NULL, INTERRUPT_CORE_AFFINITY_MASK);
    return uxCoreAffinityMask;
}

// Restores the core affinity of the executing task after calling set_interrupt_core_affinity.
void clear_interrupt_core_affinity(UBaseType_t uxCoreAffinityMask) {
    vTaskCoreAffinitySet(NULL, uxCoreAffinityMask);
}

// Returns true if the caller is executing on the designated interrupt core.
bool check_interrupt_core_affinity(void) {
    return (1u << portGET_CORE_ID()) & INTERRUPT_CORE_AFFINITY_MASK;
}

#if configUSE_IPIS
static uint32_t ipi_mask[configNUMBER_OF_CORES];

// Raises an interrupt on another core.
void send_interprocessor_interrupt(uint core_num, uint irq_num) {
    assert(core_num < configNUMBER_OF_CORES);
    taskENTER_CRITICAL();
    ipi_mask[core_num] |= 1u << irq_num;
    portYIELD_CORE(core_num);
    taskEXIT_CRITICAL();
}

__attribute__((visibility("hidden")))
void vPortTaskSwitchHook(TaskHandle_t task) {
    // Called from vTaskSwitchContext, which is effectively a critical section since ISR_LOCK is held and interrupts are disabled.
    uint core_num = portGET_CORE_ID();
    uint32_t *irq_mask = &ipi_mask[core_num];
    while (*irq_mask) {
        uint32_t irq_num = __builtin_ffs(*irq_mask);
        irq_set_pending(irq_num - 1);
        *irq_mask >>= irq_num;
    }
}
#endif
