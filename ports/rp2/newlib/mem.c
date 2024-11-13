// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/random.h>
#include <unistd.h>

#include "newlib/dev.h"
#include "newlib/mem.h"


static int mem_close(void *ctx) {
    struct mem_file *file = ctx;
    free(file);
    return 0;
}

static off_t mem_lseek(void *ctx, off_t offset, int whence) {
    struct mem_file *file = ctx;
    if (file->dev != DEV_MEM) {
        return 0;
    }
    switch (whence) {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            offset += (intptr_t)file->ptr;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    file->ptr = (void *)offset;
    return offset;
}

static int mem_pread(void *ctx, void *buffer, size_t size, off_t offset) {
    struct mem_file *file = ctx;
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    switch (file->dev) {
        case DEV_MEM:
            memcpy(buffer, (void *)offset, size);
            return size;
        case DEV_NULL:
        case DEV_ZERO:
        case DEV_FULL:
        case DEV_RANDOM:
        case DEV_URANDOM:
            errno = ESPIPE;
            return -1;
        default:
            errno = ENODEV;
            return -1;
    }
}

static int mem_read(void *ctx, void *buffer, size_t size) {
    struct mem_file *file = ctx;
    switch (file->dev) {
        case DEV_MEM:
            memcpy(buffer, file->ptr, size);
            file->ptr += size;
            return size;
        case DEV_NULL:
            return 0;
        case DEV_ZERO:
        case DEV_FULL:
            memset(buffer, 0, size);
            return size;
        case DEV_RANDOM:
        case DEV_URANDOM:
            return getrandom(buffer, size, (file->dev == DEV_RANDOM) ? GRND_RANDOM : 0);
        default:
            errno = ENODEV;
            return -1;
    }
}

static int mem_pwrite(void *ctx, const void *buffer, size_t size, off_t offset) {
    struct mem_file *file = ctx;
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    switch (file->dev) {
        case DEV_MEM:
            memcpy((void *)offset, buffer, size);
            return size;
        case DEV_NULL:
        case DEV_ZERO:
        case DEV_FULL:
        case DEV_RANDOM:
        case DEV_URANDOM:
            errno = ESPIPE;
            return -1;
        default:
            errno = ENODEV;
            return -1;
    }
}

static int mem_write(void *ctx, const void *buffer, size_t size) {
    struct mem_file *file = ctx;
    switch (file->dev) {
        case DEV_MEM:
            memcpy(file->ptr, buffer, size);
            file->ptr += size;
            return size;
        case DEV_NULL:
        case DEV_ZERO:
        case DEV_RANDOM:
        case DEV_URANDOM:
            return size;
        case DEV_FULL:
            errno = ENOSPC;
            return -1;
        default:
            errno = ENODEV;
            return -1;
    }
}

static const struct vfs_file_vtable mem_file_vtable = {
    .close = mem_close,
    // .fstat = mem_fstat,
    .lseek = mem_lseek,
    .pread = mem_pread,
    .pwrite = mem_pwrite,
    .read = mem_read,
    .write = mem_write,
};

static void *mem_open(const void *ctx, dev_t dev, int flags, mode_t mode) {
    switch (dev) {
        case DEV_MEM:
        case DEV_NULL:
        case DEV_ZERO:
        case DEV_FULL:
            break;
        default:
            errno = ENODEV;
            return NULL;
    }
    struct mem_file *file = calloc(1, sizeof(struct mem_file));
    if (!file) {
        return NULL;
    }
    vfs_file_init(&file->base, &mem_file_vtable, mode | S_IFCHR);
    file->dev = dev;
    return file;
}

const struct dev_driver mem_drv = {
    .dev = DEV_MEM,
    .open = mem_open,
};
