// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "FreeRTOS.h"

/* By convention all interrupts should execute on a particular core. This is because each core has
 * its own copy of part of the interrupt state, and it would be difficult maintain this state
 * across multiple cores. So we limit all interrupt activity to one core to avoid this problem.
 *
 * When an application needs to do something with core-specific interrupt state (e.g., call
 * irq_set_enabled), then it must switch to the interrupt core first. This can be done via the
 * set/clear interrupt_core_affinity functions. These function themselves cannot be called from an
 * interrupt context.
 */

#define INTERRUPT_CORE_NUM                0
#define INTERRUPT_CORE_AFFINITY_MASK      (1u << INTERRUPT_CORE_NUM)


UBaseType_t set_interrupt_core_affinity(void);

void clear_interrupt_core_affinity(UBaseType_t uxCoreAffinityMask);

bool check_interrupt_core_affinity(void);
