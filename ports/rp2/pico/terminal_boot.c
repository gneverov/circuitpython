// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "pico/stdlib.h"

#include "newlib/newlib.h"
#include "pico/terminal.h"


static int terminal_boot_close(void *state) {
    uart_inst_t *uart = state;
    uart_deinit(uart);
    return 0;
}

static int terminal_boot_read(void *state, char *buf, int size) {
    uart_inst_t *uart = state;
    int i = 0;
    buf[i++] = uart_getc(uart);
    while ((i < size) && uart_is_readable_within_us(uart, 100)) {
        buf[i++] = uart_getc(uart);
    }
    return i;
}

static int terminal_boot_write(void *state, const char *buf, int size) {
    uart_inst_t *uart = state;
    uart_write_blocking(uart, (const uint8_t *)buf, size);
    return size;
}

static const struct fd_vtable terminal_boot_vtable = {
    .close = terminal_boot_close,
    .read = terminal_boot_read,
    .write = terminal_boot_write,
};

int terminal_boot_open() {
    setup_default_uart();
    return fd_open(&terminal_boot_vtable, uart_default, 0);
}
