// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "freertos/interrupts.h"
#include "task.h"

__attribute__((visibility("hidden")))
UBaseType_t set_interrupt_core_affinity(void) {
    UBaseType_t uxCoreAffinityMask = vTaskCoreAffinityGet(NULL);
    vTaskCoreAffinitySet(NULL, INTERRUPT_CORE_AFFINITY_MASK);
    return uxCoreAffinityMask;
}

__attribute__((visibility("hidden")))
void clear_interrupt_core_affinity(UBaseType_t uxCoreAffinityMask) {
    vTaskCoreAffinitySet(NULL, uxCoreAffinityMask);
}

__attribute__((visibility("hidden")))
bool check_interrupt_core_affinity(void) {
    return (1u << portGET_CORE_ID()) & INTERRUPT_CORE_AFFINITY_MASK;
}
