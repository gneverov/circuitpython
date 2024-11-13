// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/dma.h"

typedef struct pico_fifo pico_fifo_t;

typedef void (*pico_fifo_handler_t)(pico_fifo_t *fifo, bool stalled);

struct pico_fifo {
    uint channel;
    uint size;
    void *buffer;
    size_t lock_count;
    volatile uint next_read;
    volatile uint next_write;
    volatile uint trans_count;
    bool tx;
    uint threshold;
    enum dma_channel_transfer_size dma_transfer_size;

    pico_fifo_handler_t handler;

    uint int_count;
};

void pico_fifo_init(pico_fifo_t *fifo, bool tx);

bool pico_fifo_alloc(pico_fifo_t *fifo, uint fifo_size, uint dreq, uint threshold, enum dma_channel_transfer_size dma_transfer_size, bool bswap, volatile void *target_addr);

void pico_fifo_deinit(pico_fifo_t *fifo);

void pico_fifo_acquire(pico_fifo_t *fifo);
void pico_fifo_release(pico_fifo_t *fifo);

void pico_fifo_sync(pico_fifo_t *fifo);

void pico_fifo_flush(pico_fifo_t *fifo);

size_t pico_fifo_available(pico_fifo_t *fifo);

size_t pico_fifo_empty(pico_fifo_t *fifo);

size_t pico_fifo_get_buffer(pico_fifo_t *fifo, void **buf);

void pico_fifo_put_buffer(pico_fifo_t *fifo, size_t bufsize);

size_t pico_fifo_transfer(pico_fifo_t *fifo, void *buf, size_t bufsize, bool flush);

void pico_fifo_clear(pico_fifo_t *fifo);

void pico_fifo_set_enabled(pico_fifo_t *fifo, bool enable);

bool pico_fifo_get_enabled(pico_fifo_t *fifo);

void pico_fifo_set_handler(pico_fifo_t *fifo, pico_fifo_handler_t handler);

#ifndef NDEBUG
void pico_fifo_debug(const pico_fifo_t *fifo);
#endif
