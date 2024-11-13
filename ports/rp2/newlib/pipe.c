// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <poll.h>
#include "newlib/newlib.h"
#include "newlib/ring.h"
#include "newlib/vfs.h"

#include "FreeRTOS.h"
#include "semphr.h"


struct pipe_file {
    struct vfs_file base;
    struct pipe *pipe;
    int closed;
};

struct pipe {
    SemaphoreHandle_t mutex;
    int ref_count;
    ring_t ring;
    struct pipe_file files[2];
    StaticSemaphore_t mutex_buffer;
};

static void pipe_lock(struct pipe *pipe) {
    xSemaphoreTake(pipe->mutex, portMAX_DELAY);
}

static void pipe_unlock(struct pipe *pipe) {
    xSemaphoreGive(pipe->mutex);
}

static int pipe_close(void *ctx) {
    struct pipe_file *file = ctx;
    struct pipe *pipe = file->pipe;
    pipe_lock(pipe);
    file->closed++;
    int ref_count = --pipe->ref_count;
    pipe_unlock(pipe);
    if (ref_count == 0) {
        vSemaphoreDelete(pipe->mutex);
        ring_free(&pipe->ring);
        free(pipe);
    }
    return 0;
}

static int pipe_fstat(void *ctx, struct stat *pstat) {
    // struct pipe_file *file = ctx;
    // struct pipe *pipe = file->pipe;
    return 0;
}

static uint pipe_poll(void *ctx) {
    struct pipe_file *file = ctx;
    struct pipe *pipe = file->pipe;
    ring_t *ring = &pipe->ring;
    uint events = 0;
    pipe_lock(pipe);
    if (ring->write_index > ring->read_index) {
        events |= POLLIN | POLLRDNORM;
    }
    if (ring->write_index < ring->read_index + ring->size) {
        events |= POLLOUT | POLLWRNORM;
    }
    pipe_unlock(pipe);
    return events;
}

static int pipe_read(void *ctx, void *buffer, size_t size) {
    struct pipe_file *file = ctx;
    struct pipe *pipe = file->pipe;
    ring_t *ring = &pipe->ring;
    pipe_lock(pipe);
    assert(!pipe->files[0].closed);
    size_t read_count = ring_read(ring, buffer, size);
    int writer_closed = pipe->files[1].closed;
    pipe_unlock(pipe);
    if (read_count) {
        poll_notify(&pipe->files[1].base, POLLOUT | POLLWRNORM);
    } else if (!writer_closed) {
        errno = EAGAIN;
        return -1;
    }
    return read_count;
}

static int pipe_write(void *ctx, const void *buffer, size_t size) {
    struct pipe_file *file = ctx;
    struct pipe *pipe = file->pipe;
    ring_t *ring = &pipe->ring;
    pipe_lock(pipe);
    assert(!pipe->files[1].closed);
    size_t write_count = ring_write(ring, buffer, size);
    int reader_closed = pipe->files[0].closed;
    pipe_unlock(pipe);
    if (reader_closed) {
        errno = EPIPE;
        return -1;
    }
    if (write_count) {
        poll_notify(&pipe->files[0].base, POLLIN | POLLRDNORM);
    }
    return write_count;
}

static const struct vfs_file_vtable pipe_vtable = {
    .close = pipe_close,
    .fstat = pipe_fstat,
    .poll = pipe_poll,
    .read = pipe_read,
    .write = pipe_write,
};

static struct pipe *pipe_open(mode_t mode) {
    struct pipe *pipe = malloc(sizeof(struct pipe));
    if (!pipe) {
        return NULL;
    }
    if (!ring_alloc(&pipe->ring, 9)) {
        free(pipe);
        pipe = NULL;
    }
    pipe->mutex = xSemaphoreCreateMutexStatic(&pipe->mutex_buffer);
    pipe->ref_count = 0;
    for (int i = 0; i < 2; i++) {
        struct pipe_file *file = &pipe->files[i];
        vfs_file_init(&file->base, &pipe_vtable, (mode & ~S_IFMT) | S_IFIFO);
        file->pipe = pipe;
        file->closed = 0;
        pipe->ref_count++;
    }
    return pipe;
}

int pipe(int fildes[2]) {
    struct pipe *pipe = pipe_open(0);
    if (!pipe) {
        return -1;
    }
    for (int i = 0; i < 2; i++) {
        fildes[i] = vfs_replace(-1, &pipe->files[i].base, FREAD + i);
        vfs_release_file(&pipe->files[i].base);
    }
    int ret = 0;
    if ((fildes[0] < 0) || (fildes[1] < 0)) {
        vfs_close(fildes[0]);
        vfs_close(fildes[1]);
        ret = -1;
    }
    return ret;
}
