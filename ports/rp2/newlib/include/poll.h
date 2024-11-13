// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/types.h>


#define POLLIN 0x0001                   // Data other than high-priority data may be read without blocking.
#define POLLRDNORM 0x0002               // Normal data may be read without blocking.
#define POLLRDBAND 0x0004               // Priority data may be read without blocking.
#define POLLPRI 0x0008                  // High priority data may be read without blocking.
#define POLLOUT 0x0010                  // Normal data may be written without blocking.
#define POLLWRNORM 0x0020               // Equivalent to POLLOUT.
#define POLLWRBAND 0x0040               // Priority data may be written.
#define POLLERR 0x0080                  // An error has occurred (revents only).
#define POLLHUP 0x0100                  // Device has been disconnected (revents only).
#define POLLNVAL 0x0200                 // Invalid fd member (revents only).

// not from POSIX
#define POLLDRAIN 0x0400                // Transmit queue is drained


struct pollfd {
    int fd;                             // The following descriptor being polled.
    short events;                       // The input event flags (see below).
    short revents;                      // The output event flags (see below).
};

typedef uint nfds_t;

int poll(struct pollfd fds[], nfds_t nfds, int timeout);
