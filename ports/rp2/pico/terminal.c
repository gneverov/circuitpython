// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>


void *terminal_boot_open(mode_t mode);
void *terminal_uart_open(const char *fragment, int flags, mode_t mode, dev_t dev);

void *terminal_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    if (flags & ~O_ACCMODE) {
        return terminal_boot_open(mode);
    } else {
        return terminal_uart_open(fragment, flags, mode, dev);
    }
}
