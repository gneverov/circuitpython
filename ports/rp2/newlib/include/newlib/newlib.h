// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "FreeRTOS.h"
#include "task.h"


struct fd_vtable {
    int (*close)(void *state);
    int (*read)(void *state, char *buf, int size);
    int (*write)(void *state, const char *buf, int size);
};

int fd_open(const struct fd_vtable *func, void *state, int flags);

int fd_close(void);

void kill_from_isr(int pid, int sig, BaseType_t *pxHigherPriorityTaskWoken);
