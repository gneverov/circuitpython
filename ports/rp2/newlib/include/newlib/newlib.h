// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <poll.h>
#include <termios.h>
#include "newlib/vfs.h"

#include "FreeRTOS.h"


void kill_from_isr(int pid, int sig, BaseType_t *pxHigherPriorityTaskWoken);

// Allow access to errno global from extension modules which don't support TLS.
int *tls_errno(void);

int poll_ticks(struct pollfd fds[], nfds_t nfds, TickType_t *pxTicksToWait);

int poll_file(struct vfs_file *file, uint events, int timeout);

void poll_notify(struct vfs_file *file, uint events);

void poll_notify_from_isr(struct vfs_file *file, uint events, BaseType_t *pxHigherPriorityTaskWoken);

void termios_init(struct termios *termios_p, speed_t speed);
