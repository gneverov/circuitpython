// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <sys/types.h>

#define GRND_RANDOM 1
#define GRND_NONBLOCK 2


ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);
