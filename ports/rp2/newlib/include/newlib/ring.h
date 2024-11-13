// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/types.h>


typedef struct ring {
    void *buffer;
    size_t size;
    size_t read_index;
    size_t write_index;
} ring_t;


void *ring_alloc(ring_t *ring, uint log2_size);

void ring_free(ring_t *ring);

static inline char *ring_at(const ring_t *ring, size_t index, size_t *size) {
    size_t offset = index & (ring->size - 1);
    *size = ring->size - offset;
    return ring->buffer + offset;
}

static inline size_t ring_write_count(const ring_t *ring) {
    return ring->read_index + ring->size - ring->write_index;
}

static inline size_t ring_read_count(const ring_t *ring) {
    return ring->write_index - ring->read_index;
}

size_t ring_write(ring_t *ring, const void *buffer, size_t buffer_size);

size_t ring_read(ring_t *ring, void *buffer, size_t buffer_size);

size_t ring_chr(ring_t *ring, int ch);

static inline void ring_clear(ring_t *ring) {
    ring->write_index = 0;
    ring->read_index = 0;
}
