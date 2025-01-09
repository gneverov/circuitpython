// SPDX-FileCopyrightText: 2025 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "morelib/poll.h"

#include "py/obj.h"


typedef struct {
    struct poll_file *file;
    int fd;
} mp_poll_t;

void mp_poll_init(mp_poll_t *self);
int mp_poll_alloc(mp_poll_t *self, uint events);
void mp_poll_deinit(mp_poll_t *self);
int mp_poll_fileno(mp_poll_t *self);
bool mp_poll_wait(mp_poll_t *self, uint events, TickType_t *pxTicksToWait);
