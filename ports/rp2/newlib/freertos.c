// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "FreeRTOS.h"
 #include "task.h"


uint32_t __atomic_fetch_add_4(volatile void *mem, uint32_t val, int model) {
    taskENTER_CRITICAL();
    uint32_t old_val = *(uint32_t *)mem += val;
    taskEXIT_CRITICAL();
    return old_val;
}
