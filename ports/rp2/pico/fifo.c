// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>
#include <memory.h>

#include "hardware/timer.h"

#include "pico/dma.h"
#include "pico/fifo.h"

static void pico_fifo_irq_handler(uint channel, void *context);

void pico_fifo_init(pico_fifo_t *fifo, bool tx) {
    fifo->channel = -1u;
    fifo->size = 0;
    fifo->buffer = NULL;
    fifo->next_read = 0;
    fifo->next_write = 0;
    fifo->trans_count = 0;
    fifo->tx = tx;
    fifo->threshold = 0;
    fifo->dma_transfer_size = DMA_SIZE_8;
    fifo->int_count = 0;
}

bool pico_fifo_alloc(pico_fifo_t *fifo, uint fifo_size, uint dreq, uint threshold, enum dma_channel_transfer_size dma_transfer_size, bool bswap, volatile void *target_addr) {
    uint channel = dma_claim_unused_channel(false);
    if (channel == -1u) {
        errno = EBUSY;
        return false;
    }

    void *buffer = malloc(fifo_size);
    if (!buffer) {
        errno = ENOMEM;
        return false;
    }

    fifo->channel = channel;
    fifo->size = fifo_size;
    fifo->buffer = buffer;
    fifo->threshold = threshold ? threshold : fifo_size >> 2;
    fifo->dma_transfer_size = dma_transfer_size;

    dma_channel_config c = dma_channel_get_default_config(channel);
    channel_config_set_read_increment(&c, fifo->tx);
    channel_config_set_write_increment(&c, !fifo->tx);
    channel_config_set_dreq(&c, dreq);
    channel_config_set_transfer_data_size(&c, dma_transfer_size);
    channel_config_set_bswap(&c, bswap);
    dma_channel_set_config(channel, &c, false);

    dma_channel_set_trans_count(channel, 0, false);
    if (fifo->tx) {
        dma_channel_set_read_addr(channel, fifo->buffer, false);
        dma_channel_set_write_addr(channel, target_addr, false);
    } else {
        dma_channel_set_read_addr(channel, target_addr, false);
        dma_channel_set_write_addr(channel, fifo->buffer, false);
    }
    pico_fifo_release(fifo);
    pico_fifo_flush(fifo);
    return true;
}

void pico_fifo_deinit(pico_fifo_t *fifo) {
    if (fifo->channel != -1u) {
        // pico_fifo_set_enabled(fifo, false);
        pico_fifo_acquire(fifo);
        dma_channel_abort(fifo->channel);
        // busy_wait_us(1000);
        pico_dma_acknowledge_irq(fifo->channel);
        dma_channel_unclaim(fifo->channel);
        fifo->channel = -1u;
    }
    if (fifo->buffer) {
        free(fifo->buffer);
        fifo->buffer = NULL;
    }
}

void pico_fifo_acquire(pico_fifo_t *fifo) {
    pico_dma_clear_irq(fifo->channel);
}

void pico_fifo_release(pico_fifo_t *fifo) {
    pico_dma_set_irq(fifo->channel, pico_fifo_irq_handler, fifo);
}

static uint pico_dma_read_trans_count(pico_fifo_t *fifo) {
    uint count = fifo->next_write - fifo->next_read;
    if (fifo->tx) {
        count = fifo->size - count;
    }

    uint trans_count = dma_channel_hw_addr(fifo->channel)->transfer_count;
    uint delta = (fifo->trans_count - trans_count) << fifo->dma_transfer_size;
    if (fifo->tx) {
        fifo->next_read += delta;
        if ((fifo->next_read % fifo->size) == 0) {
            dma_channel_set_read_addr(fifo->channel, fifo->buffer, false);
        }
    } else {
        fifo->next_write += delta;
        if ((fifo->next_write % fifo->size) == 0) {
            dma_channel_set_write_addr(fifo->channel, fifo->buffer, false);
        }
    }
    fifo->trans_count = trans_count;
    return fifo->threshold - count <= delta;
}

static uint pico_dma_write_trans_count(pico_fifo_t *fifo) {
    uint index = fifo->next_read;
    uint count = fifo->next_write - fifo->next_read;
    if (!fifo->tx) {
        index = fifo->next_write;
        count = fifo->size - count;
    }
    index %= fifo->size;
    count = MIN(count, fifo->size - index);

    uint trans_count = MIN(count, fifo->threshold) >> fifo->dma_transfer_size;
    fifo->trans_count = trans_count;
    if (trans_count) {
        dma_channel_set_trans_count(fifo->channel, trans_count, true);
    }
    return count;
}

static void pico_fifo_irq_handler(uint channel, void *context) {
    pico_fifo_t *fifo = context;
    pico_dma_acknowledge_irq(fifo->channel);
    fifo->int_count++;

    uint available = pico_dma_read_trans_count(fifo);
    uint ready = pico_dma_write_trans_count(fifo);
    if (fifo->handler) {
        if (available) {
            fifo->handler(fifo, false);
        }
        if (!ready) {
            fifo->handler(fifo, true);
        }
    }
}

void pico_fifo_sync(pico_fifo_t *fifo) {
    pico_fifo_acquire(fifo);
    if (fifo->trans_count) {
        pico_dma_read_trans_count(fifo);
    }
    pico_fifo_release(fifo);
}

void pico_fifo_flush(pico_fifo_t *fifo) {
    pico_fifo_acquire(fifo);
    if (!fifo->trans_count) {
        pico_dma_write_trans_count(fifo);
    }
    pico_fifo_release(fifo);
}

size_t pico_fifo_available(pico_fifo_t *fifo) {
    uint count = fifo->next_write - fifo->next_read;
    if (fifo->tx) {
        count = fifo->size - count;
    }
    return count;
}

size_t pico_fifo_empty(pico_fifo_t *fifo) {
    return fifo->next_write == fifo->next_read;
}

size_t pico_fifo_get_buffer(pico_fifo_t *fifo, void **buf) {
    uint next_read = fifo->next_read;
    uint next_write = fifo->next_write;
    uint index = next_read;
    uint count = next_write - next_read;
    if (fifo->tx) {
        index = next_write;
        count = fifo->size - count;
    }
    index %= fifo->size;
    *buf = fifo->buffer + index;
    return MIN(count, fifo->size - index);
}

void pico_fifo_put_buffer(pico_fifo_t *fifo, size_t bufsize) {
    pico_fifo_acquire(fifo);
    if (fifo->tx) {
        fifo->next_write += bufsize;
    } else {
        fifo->next_read += bufsize;
    }
    uint count = fifo->next_write - fifo->next_read;
    if (!fifo->tx) {
        count = fifo->size - count;
    }
    if ((count >= fifo->threshold) && !fifo->trans_count) {
        pico_dma_write_trans_count(fifo);
    }
    pico_fifo_release(fifo);
}

size_t pico_fifo_transfer(pico_fifo_t *fifo, void *buffer, size_t size, bool flush) {
    void *ring;
    void **dst = fifo->tx ? &ring : &buffer;
    void **src = fifo->tx ? &buffer : &ring;

    pico_fifo_acquire(fifo);
    if (flush && !fifo->tx && fifo->trans_count) {
        pico_dma_read_trans_count(fifo);
    }
    size_t total = 0;
    while (total < size) {
        size_t count = MIN(size - total, pico_fifo_get_buffer(fifo, &ring));
        if (!count) {
            break;
        }
        memcpy(*dst, *src, count);
        pico_fifo_put_buffer(fifo, count);
        buffer += count;
        total += count;
    }
    if (flush && fifo->tx && !fifo->trans_count) {
        pico_dma_write_trans_count(fifo);
    }
    pico_fifo_release(fifo);
    return total;
}

void pico_fifo_clear(pico_fifo_t *fifo) {
    pico_fifo_acquire(fifo);
    dma_channel_abort(fifo->channel);
    pico_dma_acknowledge_irq(fifo->channel);

    fifo->next_read = 0;
    fifo->next_write = 0;
    if (fifo->tx) {
        dma_channel_set_read_addr(fifo->channel, fifo->buffer, false);
    } else {
        dma_channel_set_write_addr(fifo->channel, fifo->buffer, false);
    }
    fifo->trans_count = 0;
    dma_channel_set_trans_count(fifo->channel, 0, false);

    pico_fifo_release(fifo);

    pico_fifo_flush(fifo);
}

void pico_fifo_set_enabled(pico_fifo_t *fifo, bool enable) {
    dma_channel_config c = dma_get_channel_config(fifo->channel);
    channel_config_set_enable(&c, enable);
    dma_channel_set_config(fifo->channel, &c, enable);
}

void pico_fifo_set_handler(pico_fifo_t *fifo, pico_fifo_handler_t handler) {
    pico_fifo_acquire(fifo);
    fifo->handler = handler;
    pico_fifo_release(fifo);
}

#ifndef NDEBUG
#include <stdio.h>

void pico_fifo_debug(const pico_fifo_t *fifo) {
    printf("pico_fifo %p\n", fifo);
    printf("  tx:          %d\n", fifo->tx);
    printf("  buffer       %p\n", fifo->buffer);
    printf("  size:        %u\n", fifo->size);
    printf("  next_read:   %u (0x%04x)\n", fifo->next_read, fifo->next_read % fifo->size);
    printf("  next_write:  %u (0x%04x)\n", fifo->next_write, fifo->next_write % fifo->size);
    printf("  trans_count: %u\n", fifo->trans_count);
    printf("  threshold:   %u\n", fifo->threshold);
    printf("  int_count:   %u\n", fifo->int_count);

    if (fifo->channel != -1u) {
        pico_dma_debug(fifo->channel);
    }
}
#endif
