// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>
#include <signal.h>

#include "FreeRTOS.h"
#include "event_groups.h"

#include "freertos/task_helper.h"
#include "newlib/newlib.h"
#include "newlib/poll.h"
#include "pico/uart.h"

#include "pico/terminal.h"


typedef struct {
    pico_uart_t uart;
    EventGroupHandle_t events;
    StaticEventGroup_t events_buffer;
} terminal_uart_t;

static void terminal_uart_handler(pico_uart_t *uart, uint events) {
    static_assert(offsetof(terminal_uart_t, uart) == 0);
    terminal_uart_t *self = (terminal_uart_t *)uart;
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

static int terminal_uart_read(void *state, char *buf, int size) {
    terminal_uart_t *self = state;
    int br = pico_uart_read(&self->uart, buf, size);
    while (br == 0) {
        if (task_check_interrupted()) {
            return -1;
        }

        task_enable_interrupt();
        xEventGroupWaitBits(self->events, ~POLLOUT & 0xff, pdTRUE, pdFALSE, portMAX_DELAY);
        task_disable_interrupt();

        br = pico_uart_read(&self->uart, buf, size);
    }
    return br;
}

static int terminal_uart_write(void *state, const char *buf, int size) {
    terminal_uart_t *self = state;
    int total = 0;
    while (total < size) {
        int bw = pico_uart_write(&self->uart, buf + total, size - total);
        if (bw == 0) {
            if (task_check_interrupted()) {
                return -1;
            }

            task_enable_interrupt();
            xEventGroupWaitBits(self->events, ~POLLIN & 0xff, pdTRUE, pdFALSE, portMAX_DELAY);
            task_disable_interrupt();
        } else if (bw > 0) {
            total += bw;
        } else {
            return bw;
        }
    }
    return total;
}

static const struct fd_vtable terminal_uart_vtable = {
    .close = terminal_uart_close,
    .read = terminal_uart_read,
    .write = terminal_uart_write,
};

int terminal_uart_open() {
    terminal_uart_t *self = malloc(sizeof(terminal_uart_t));
    if (!self) {
        errno = ENOMEM;
        return -1;
    }

    self->events = xEventGroupCreateStatic(&self->events_buffer);

    if (!pico_uart_init(&self->uart, PICO_DEFAULT_UART_INSTANCE, PICO_DEFAULT_UART_TX_PIN, PICO_DEFAULT_UART_RX_PIN, PICO_DEFAULT_UART_BAUD_RATE, terminal_uart_handler)) {
        return -1;
    }

    return fd_open(&terminal_uart_vtable, self, 0);
}
