// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "FreeRTOS.h"

void env_init(void);

void kill_from_isr(int pid, int sig, BaseType_t *pxHigherPriorityTaskWoken);

// Allow access to errno global from extension modules which don't support TLS.
int *tls_errno(void);
