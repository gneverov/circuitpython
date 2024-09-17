// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <sys/time.h>

#include "FreeRTOS.h"
#include "task.h"

#include "newlib/flash.h"
#include "newlib/flash_dev.h"
#include "newlib/flash_lockout.h"
#include "newlib/ioctl.h"
#include "newlib/newlib.h"
#include "newlib/vfs.h"

struct flash_file {
    struct vfs_file base;
    off_t ptr;
    int flags;
};

static struct timespec flash_mtime;

static int flash_close(void *ctx) {
    struct flash_file *file = ctx;
    free(file);
    return 0;
}

static int flash_fstat(void *ctx, struct stat *pstat) {
    pstat->st_size = flash_storage_size;
    pstat->st_blksize = FLASH_SECTOR_SIZE;
    vTaskSuspendAll();
    pstat->st_mtim = flash_mtime;
    xTaskResumeAll();
    return 0;
}

static int flash_ioctl(void *ctx, unsigned long request, va_list args) {
    struct flash_file *file = ctx;
    switch (request) {
        case BLKROSET: {
            const int *ro = va_arg(args, const int *);
            file->flags = (file->flags & ~O_ACCMODE) | (*ro ? O_RDONLY : O_RDWR);
            return 0;
        }

        case BLKROGET: {
            int *ro = va_arg(args, int *);
            *ro = (file->flags & O_ACCMODE) == O_RDONLY;
            return 0;
        }

        case BLKGETSIZE: {
            unsigned long *size = va_arg(args, unsigned long *);
            *size = flash_storage_size >> 9;
            return 0;
        }

        case BLKFLSBUF: {
            return 0;
        }

        case BLKSSZGET: {
            int *ssize = va_arg(args, int *);
            *ssize = FLASH_SECTOR_SIZE;
            return 0;
        }

        default: {
            errno = EINVAL;
            return -1;
        }
    }
}

static off_t flash_lseek(void *ctx, off_t pos, int whence) {
    struct flash_file *file = ctx;
    off_t ptr;
    switch (whence) {
        case SEEK_SET:
            ptr = 0;
            break;
        case SEEK_CUR:
            ptr = file->ptr;
            break;
        case SEEK_END:
            ptr = flash_storage_size;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    ptr += pos;
    if (ptr < 0) {
        errno = EINVAL;
        return -1;
    }
    if (ptr > flash_storage_size) {
        errno = EFBIG;
        return -1;
    }
    file->ptr = (ptr + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    return file->ptr;
}

static int flash_read(void *ctx, void *buf, size_t size) {
    struct flash_file *file = ctx;
    size = MIN(size, flash_storage_size - file->ptr);
    vTaskSuspendAll();
    flash_memread(flash_storage_offset + file->ptr, buf, size);
    xTaskResumeAll();
    file->ptr += (size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    return size;
}

static int flash_write(void *ctx, const void *buf, size_t size) {
    struct flash_file *file = ctx;
    if (file->ptr + size > flash_storage_size) {
        errno = EFBIG;
        return -1;
    }
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        errno = EROFS;
        return -1;
    }

    struct timeval t;
    gettimeofday(&t, NULL);
    flash_lockout_start();
    flash_memwrite(flash_storage_offset + file->ptr, buf, size);
    flash_mtime.tv_sec = t.tv_sec;
    flash_mtime.tv_nsec = t.tv_usec * 1000;
    flash_lockout_end();

    file->ptr += (size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    return size;
}

static const struct vfs_file_vtable flash_vtable = {
    .close = flash_close,
    .fstat = flash_fstat,
    .ioctl = flash_ioctl,
    .lseek = flash_lseek,
    .read = flash_read,
    .write = flash_write,
};

void *flash_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    struct flash_file *file = malloc(sizeof(struct flash_file));
    vfs_file_init(&file->base, &flash_vtable, mode);
    file->ptr = 0;
    file->flags = flags;
    return file;
}
