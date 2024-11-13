// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <memory.h>
#include <sys/param.h>
#include "newlib/ring.h"


void *ring_alloc(ring_t *ring, uint log2_size) {
    size_t size = 1u << log2_size;
    ring->buffer = malloc(size);
    ring->size = size;
    ring->write_index = 0;
    ring->read_index = 0;
    return ring->buffer;
}

void ring_free(ring_t *ring) {
    if (ring->buffer) {
        free(ring->buffer);
        ring->buffer = NULL;
    }
}

size_t ring_write(ring_t *ring, const void *buffer, size_t buffer_size) {
    size_t max_index = ring->read_index + ring->size;
    size_t write_count = 0;
    while ((ring->write_index < max_index) && (write_count < buffer_size)) {
        size_t size;
        void *write_ptr = ring_at(ring, ring->write_index, &size);
        size = MIN(size, max_index - ring->write_index);
        size = MIN(size, buffer_size - write_count);
        memcpy(write_ptr, buffer + write_count, size);
        ring->write_index += size;
        write_count += size;
    }
    return write_count;
}

size_t ring_read(ring_t *ring, void *buffer, size_t buffer_size) {
    size_t max_index = ring->write_index;
    size_t read_count = 0;
    while ((ring->read_index < max_index) && (read_count < buffer_size)) {
        size_t size;
        const void *read_ptr = ring_at(ring, ring->read_index, &size);
        size = MIN(size, max_index - ring->read_index);
        size = MIN(size, buffer_size - read_count);
        memcpy(buffer + read_count, read_ptr, size);
        ring->read_index += size;
        read_count += size;
    }
    return read_count;
}

size_t ring_chr(ring_t *ring, int ch) {
    size_t read_index = ring->read_index;
    size_t max_index = ring->write_index;
    while (read_index < max_index) {
        size_t size;
        const void *read_ptr = ring_at(ring, read_index, &size);
        size = MIN(size, max_index - read_index);
        const void *ch_ptr = memchr(read_ptr, ch, size);
        if (ch_ptr) {
            read_index += ch_ptr - read_ptr;
            break;
        }
        read_index += size;
    }
    return read_index;
}
