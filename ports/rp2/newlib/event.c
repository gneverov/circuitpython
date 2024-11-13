// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <poll.h>
#include "newlib/newlib.h"
#include "newlib/vfs.h"

#include "FreeRTOS.h"
#include "task.h"


struct event_file {
    struct vfs_file base;
    uint events;
};

static uint event_poll(void *ctx) {
    struct event_file *event = ctx;
    taskENTER_CRITICAL();
    uint events = event->events;
    taskEXIT_CRITICAL();
    return events;
}

static int event_read(void *ctx, void *buffer, size_t size) {
    struct event_file *file = ctx;
    taskENTER_CRITICAL();
    uint32_t events = file->events;
    taskEXIT_CRITICAL();

    if (events & POLLIN) {
        return size;
    } else {
        errno = EAGAIN;
        return -1;
    }
}

static int event_write(void *ctx, const void *buffer, size_t size) {
    struct event_file *file = ctx;
    taskENTER_CRITICAL();
    uint32_t events = file->events;
    taskEXIT_CRITICAL();

    if (events & POLLOUT) {
        return size;
    } else {
        errno = EAGAIN;
        return -1;
    }
}

static const struct vfs_file_vtable event_vtable = {
    .poll = event_poll,
    .read = event_read,
    .write = event_write,
};

int event_open(uint32_t events, int flags) {
    struct event_file *file = malloc(sizeof(struct event_file));
    if (!file) {
        return -1;
    }
    vfs_file_init(&file->base, &event_vtable, 0);
    file->events = events;
    int ret = vfs_replace(-1, &file->base, (flags & ~O_ACCMODE) | FREAD | FWRITE);
    vfs_release_file(&file->base);
    return ret;
}

struct event_file *event_fdopen(int fd) {
    int flags = 0;
    struct event_file *file = (void *)vfs_acquire_file(fd, &flags);
    if (!file) {
        errno = EBADF;
    } else if (file->base.func != &event_vtable) {
        vfs_release_file(&file->base);
        file = NULL;
        errno = EBADF;
    }
    return file;
}

int event_wait(int fd, uint events) {
    int flags = 0;
    struct event_file *file = (void *)vfs_acquire_file(fd, &flags);
    if (!file || (file->base.func != &event_vtable)) {
        errno = EBADF;
        return -1;
    }
    uint32_t file_events = event_poll(file);
    int ret;
    if (file_events & events) {
        ret = 0;
    } else if (flags & FNONBLOCK) {
        errno = EAGAIN;
        ret = -1;
    } else {
        ret = poll_file(&file->base, events, -1);
    }
    vfs_release_file(&file->base);
    return ret;
}

void event_notify(struct event_file *file, uint clear_events, uint set_events) {
    taskENTER_CRITICAL();
    uint32_t old_events = file->events;
    file->events &= ~clear_events;
    file->events |= set_events;
    uint32_t new_events = file->events;
    taskEXIT_CRITICAL();

    if (new_events & ~old_events) {
        poll_notify(&file->base, new_events);
    }
}

void event_notify_from_isr(struct event_file *file, uint clear_events, uint set_events, BaseType_t *pxHigherPriorityTaskWoken) {
    UBaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    uint32_t old_events = file->events;
    file->events &= ~clear_events;
    file->events |= set_events;
    uint32_t new_events = file->events;
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);

    if (new_events & ~old_events) {
        poll_notify_from_isr(&file->base, new_events, pxHigherPriorityTaskWoken);
    }
}
