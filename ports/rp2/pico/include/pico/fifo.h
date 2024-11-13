// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "newlib/ring.h"

#include "FreeRTOS.h"

#include "hardware/dma.h"


typedef struct pico_fifo pico_fifo_t;

typedef void (*pico_fifo_handler_t)(pico_fifo_t *fifo, const ring_t *ring, BaseType_t *pxHigherPriorityTaskWoken);

struct pico_fifo {
    ring_t ring;
    int channel;
    uint trans_count;
    enum dma_channel_transfer_size transfer_size;
    bool tx;
    pico_fifo_handler_t handler;
    uint int_count;
};

void pico_fifo_init(pico_fifo_t *fifo, bool tx, pico_fifo_handler_t handler);

bool pico_fifo_alloc(pico_fifo_t *fifo, uint fifo_size, uint dreq, enum dma_channel_transfer_size dma_transfer_size, bool bswap, volatile void *target_addr);

void pico_fifo_deinit(pico_fifo_t *fifo);

void pico_fifo_exchange(pico_fifo_t *fifo, ring_t *ring, size_t usr_count);

void pico_fifo_exchange_from_isr(pico_fifo_t *fifo, ring_t *ring, size_t usr_count);

size_t pico_fifo_transfer(pico_fifo_t *fifo, void *buffer, size_t size);

void pico_fifo_clear(pico_fifo_t *fifo);

void pico_fifo_set_enabled(pico_fifo_t *fifo, bool enable);

// bool pico_fifo_get_enabled(pico_fifo_t *fifo);

#ifndef NDEBUG
void pico_fifo_debug(const pico_fifo_t *fifo);
#endif
