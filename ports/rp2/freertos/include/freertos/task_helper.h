// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT
#pragma once

#include "FreeRTOS.h"
#include "task.h"

#define TLS_INDEX_REENT 0
#define TLS_INDEX_INTERRUPT 1
#define TLS_INDEX_APP 2

static_assert(TLS_INDEX_APP < configNUM_THREAD_LOCAL_STORAGE_POINTERS);

void task_init();

void task_deinit();

struct _reent *task_get_reent(TaskHandle_t task);

void task_enable_interrupt();

void task_disable_interrupt();

BaseType_t task_interrupt(TaskHandle_t task);

int task_check_interrupted();
