// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "pico/stdlib.h"

#include "newlib/newlib.h"
#include "newlib/vfs.h"
#include "pico/terminal.h"

struct terminal_boot {
    struct vfs_file base;
    uart_inst_t *uart;
};

static int terminal_boot_close(void *state) {
    // uart_inst_t *uart = ((struct terminal_boot *)state)->uart;
    // uart_deinit(uart);
    return 0;
}

static int terminal_boot_read(void *state, void *buf, size_t size) {
    uart_inst_t *uart = ((struct terminal_boot *)state)->uart;
    char *cbuf = buf;
    int i = 0;
    cbuf[i++] = uart_getc(uart);
    while ((i < size) && uart_is_readable_within_us(uart, 100)) {
        cbuf[i++] = uart_getc(uart);
    }
    return i;
}

static int terminal_boot_write(void *state, const void *buf, size_t size) {
    uart_inst_t *uart = ((struct terminal_boot *)state)->uart;
    uart_write_blocking(uart, (const uint8_t *)buf, size);
    return size;
}

static const struct vfs_file_vtable terminal_boot_vtable = {
    .close = terminal_boot_close,
    .isatty = 1,
    .read = terminal_boot_read,
    .write = terminal_boot_write,
};

static struct terminal_boot terminal_boot;

void *terminal_boot_open(mode_t mode) {
    setup_default_uart();
    vfs_file_init(&terminal_boot.base, &terminal_boot_vtable, mode | S_IFCHR);
    terminal_boot.uart = uart_default;
    return &terminal_boot;
}
