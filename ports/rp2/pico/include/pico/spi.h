// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/irq.h"
#include "hardware/spi.h"

#include "pico/fifo.h"

typedef struct pico_spi pico_spi_t;

typedef void (*pico_spi_handler_t)(pico_spi_t *self, uint events);

struct pico_spi {
    spi_inst_t *spi;
    uint rx_pin, sck_pin, tx_pin;
    irq_handler_t irq_handler;
    pico_fifo_t rx_fifo;
    pico_fifo_t tx_fifo;
    pico_spi_handler_t handler;
};

bool pico_spi_init(pico_spi_t *self, spi_inst_t *spi, uint tx_pin, uint rx_pin, uint baudrate, pico_spi_handler_t handler);

void pico_spi_deinit(pico_spi_t *self);

size_t pico_spi_read(pico_spi_t *self, char *buffer, size_t size);

size_t pico_spi_write(pico_spi_t *self, const char *buffer, int size);
