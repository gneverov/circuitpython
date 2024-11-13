// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <sys/random.h>


__attribute__((weak))
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
    errno = ENOSYS;
    return -1;
}
