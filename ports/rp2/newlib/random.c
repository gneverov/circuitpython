// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <memory.h>
#include <sys/random.h>
#include "pico/rand.h"


ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
    size_t remaining = buflen;
    while (remaining > 0) {
        uint32_t r = get_rand_32();
        size_t n = MIN(sizeof(uint32_t), remaining);
        memcpy(buf, &r, n);
        buf += n;
        remaining -= n;
    }
    return buflen;
}
