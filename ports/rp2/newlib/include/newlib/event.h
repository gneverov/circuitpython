// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "newlib/vfs.h"

#include "FreeRTOS.h"


struct event_file {
    struct vfs_file base;
    uint events;
};

int event_open(uint32_t events, int flags);

struct event_file *event_fdopen(int fd);

int event_wait(int fd, uint events);

void event_notify(struct event_file *file, uint clear_events, uint set_events);

void event_notify_from_isr(struct event_file *file, uint clear_events, uint set_events, BaseType_t *pxHigherPriorityTaskWoken);
