// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "event_groups.h"

#include "newlib/thread.h"
#include "newlib/newlib.h"
#include "newlib/poll.h"
#include "newlib/vfs.h"
#include "pico/uart.h"

#include "pico/terminal.h"


typedef struct {
    struct vfs_file base;
    pico_uart_t uart;
    EventGroupHandle_t events;
    StaticEventGroup_t events_buffer;
} terminal_uart_t;

static void terminal_uart_handler(pico_uart_t *uart, uint events) {
    terminal_uart_t *self = (terminal_uart_t *)((char *)uart - offsetof(terminal_uart_t, uart));
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(self->events, events, &xHigherPriorityTaskWoken);
    if (events & POLLPRI) {
        kill_from_isr(0, SIGINT, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static int terminal_uart_close(void *state) {
    terminal_uart_t *self = state;
    pico_uart_deinit(&self->uart);
    vEventGroupDelete(self->events);
    free(self);
    return 0;
}

static int terminal_uart_read(void *state, void *buf, size_t size) {
    terminal_uart_t *self = state;
    int br = pico_uart_read(&self->uart, buf, size);
    while (br == 0) {
        if (thread_enable_interrupt()) {
            return -1;
        }
        xEventGroupWaitBits(self->events, ~POLLOUT & 0xff, pdTRUE, pdFALSE, portMAX_DELAY);
        thread_disable_interrupt();

        br = pico_uart_read(&self->uart, buf, size);
    }
    return br;
}

static int terminal_uart_write(void *state, const void *buf, size_t size) {
    terminal_uart_t *self = state;
    int total = 0;
    while (total < size) {
        int bw = pico_uart_write(&self->uart, buf + total, size - total);
        if (bw == 0) {
            if (thread_enable_interrupt()) {
                return total > 0 ? total : -1;
            }
            xEventGroupWaitBits(self->events, ~POLLIN & 0xff, pdTRUE, pdFALSE, portMAX_DELAY);
            thread_disable_interrupt();
        } else if (bw > 0) {
            total += bw;
        } else {
            return bw;
        }
    }
    return total;
}

static const struct vfs_file_vtable terminal_uart_vtable = {
    .close = terminal_uart_close,
    .isatty = 1,
    .read = terminal_uart_read,
    .write = terminal_uart_write,
};

void *terminal_uart_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    uart_inst_t *uart = uart_default;
    switch (dev) {
        case DEV_TTYS0:
            uart = uart0;
            break;
        case DEV_TTYS1:
            uart = uart1;
            break;
        default:
            errno = ENODEV;
            return NULL;
    }
    uint tx_pin = PICO_DEFAULT_UART_TX_PIN;
    uint rx_pin = PICO_DEFAULT_UART_RX_PIN;
    uint baudrate = PICO_DEFAULT_UART_BAUD_RATE;
    if (fragment && (sscanf(fragment, "?tx_pin=%d,rx_pin=%d,baudrate=%d", &tx_pin, &rx_pin, &baudrate) < 0)) {
        errno = EINVAL;
        return NULL;
    }

    terminal_uart_t *self = malloc(sizeof(terminal_uart_t));
    if (!self) {
        errno = ENOMEM;
        return NULL;
    }
    vfs_file_init(&self->base, &terminal_uart_vtable, mode | S_IFCHR);
    self->events = xEventGroupCreateStatic(&self->events_buffer);

    if (!pico_uart_init(&self->uart, uart, tx_pin, rx_pin, baudrate, terminal_uart_handler)) {
        errno = EIO;
        vfs_release_file(&self->base);
        self = NULL;
    }
    return self;
}
