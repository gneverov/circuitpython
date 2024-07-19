// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>

#include "hardware/flash.h"
#include "pico/flash.h"

#include "FreeRTOS.h"
#include "task.h"

#include "newlib/flash.h"
#include "newlib/ioctl.h"
#include "newlib/newlib.h"
#include "newlib/vfs.h"

struct flash_file {
    struct vfs_file base;
    uint8_t *ptr;
    int flags;
};

extern uint8_t __flash_storage_start[];
extern uint8_t __flash_storage_end[];

static int flash_close(void *ctx) {
    struct flash_file *file = ctx;
    free(file);
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
            *size = (__flash_storage_end - __flash_storage_start) >> 9;
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
    uint8_t *ptr;
    switch (whence) {
        case SEEK_SET:
            ptr = __flash_storage_start;
            break;
        case SEEK_CUR:
            ptr = file->ptr;
            break;
        case SEEK_END:
            ptr = __flash_storage_end;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    ptr += pos;
    if (ptr < __flash_storage_start) {
        errno = EINVAL;
        return -1;
    }
    if (ptr > __flash_storage_end) {
        errno = EFBIG;
        return -1;
    }
    file->ptr += (ptr - file->ptr + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    return file->ptr - __flash_storage_start;
}

static int flash_read(void *ctx, void *buf, size_t size) {
    struct flash_file *file = ctx;
    size = MIN(size, __flash_storage_end - file->ptr);
    vTaskSuspendAll();
    memcpy(buf, file->ptr, size);
    xTaskResumeAll();
    file->ptr += (size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    return size;
}

static int flash_write(void *ctx, const void *buf, size_t size) {
    struct flash_file *file = ctx;
    if (file->ptr + size > __flash_storage_end) {
        errno = EFBIG;
        return -1;
    }
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        errno = EROFS;
        return -1;
    }

    uint32_t flash_offs = file->ptr - (uint8_t *)XIP_BASE;
    size_t sector_size = (size + FLASH_SECTOR_SIZE - 1) & ~(FLASH_SECTOR_SIZE - 1);
    size_t page_size = (size + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);

    flash_lockout_start();
    flash_range_erase(flash_offs, sector_size);
    flash_range_program(flash_offs, buf, page_size);
    flash_lockout_end();

    file->ptr += sector_size;
    return size;
}

static const struct vfs_file_vtable flash_vtable = {
    .close = flash_close,
    .ioctl = flash_ioctl,
    .lseek = flash_lseek,
    .read = flash_read,
    .write = flash_write,
};

void *flash_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    struct flash_file *file = malloc(sizeof(struct flash_file));
    vfs_file_init(&file->base, &flash_vtable, mode);
    file->ptr = __flash_storage_start;
    file->flags = flags;
    return file;
}

void flash_lockout_start(void) {
    flash_safety_helper_t *f = get_flash_safety_helper();
    if (!f->enter_safe_zone_timeout_ms(INT32_MAX)) {
        assert(0);
    }
}

void flash_lockout_end(void) {
    flash_safety_helper_t *f = get_flash_safety_helper();
    if (!f->exit_safe_zone_timeout_ms(INT32_MAX)) {
        assert(0);
    }
}
