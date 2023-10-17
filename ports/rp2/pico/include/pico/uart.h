// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/irq.h"
#include "hardware/uart.h"

#include "pico/fifo.h"

typedef struct pico_uart pico_uart_t;

typedef void (*pico_uart_handler_t)(pico_uart_t *self, uint events);

struct pico_uart {
    uart_inst_t *uart;
    uint tx_pin, rx_pin;
    irq_handler_t irq_handler;
    char *rx_buffer;
    size_t rx_buffer_size;
    size_t rx_read_index;
    size_t rx_write_index;
    pico_fifo_t tx_fifo;
    pico_uart_handler_t handler;
};

bool pico_uart_init(pico_uart_t *self, uart_inst_t *uart, uint tx_pin, uint rx_pin, uint baudrate, pico_uart_handler_t handler);

void pico_uart_deinit(pico_uart_t *self);

size_t pico_uart_read(pico_uart_t *self, char *buffer, size_t size);

size_t pico_uart_write(pico_uart_t *self, const char *buffer, int size);
