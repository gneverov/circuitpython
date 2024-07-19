// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <stdio.h>

#include "hardware/flash.h"

#include "FreeRTOS.h"
#include "task.h"

#include "dhara/map.h"
#include "dhara/nand.h"

#include "newlib/flash.h"
#include "newlib/ioctl.h"
#include "newlib/vfs.h"

#define DHARA_SECTOR_SIZE 512
static_assert(DHARA_SECTOR_SIZE >= FLASH_PAGE_SIZE);
static_assert(DHARA_SECTOR_SIZE < FLASH_SECTOR_SIZE);
static_assert((DHARA_SECTOR_SIZE & (DHARA_SECTOR_SIZE - 1)) == 0);

#define DHARA_GC_RATIO 7

struct dhara_file {
    struct vfs_file base;
    int flags;
    struct dhara_nand nor;
    struct dhara_map map;
    dhara_sector_t pos;
    dhara_sector_t limit;
    uint8_t page_buf[DHARA_SECTOR_SIZE];
};

extern uint8_t __flash_storage_start[];
extern uint8_t __flash_storage_end[];

static int dhara_check_ret(int ret, dhara_error_t err) {
    if (ret < 0) {
        errno = EIO;
        return -1;
    }
    return 0;
}

static int dhara_close(void *ctx) {
    struct dhara_file *file = ctx;
    dhara_error_t err;
    int ret = dhara_map_sync(&file->map, &err);
    free(file);
    return dhara_check_ret(ret, err);
}

static int dhara_ioctl(void *ctx, unsigned long request, va_list args) {
    struct dhara_file *file = ctx;
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
            *size = (file->limit * DHARA_SECTOR_SIZE) >> 9;
            return 0;
        }

        case BLKFLSBUF: {
            dhara_error_t err;
            int ret = dhara_map_sync(&file->map, &err);
            return dhara_check_ret(ret, err);
        }

        case BLKSSZGET: {
            int *ssize = va_arg(args, int *);
            *ssize = DHARA_SECTOR_SIZE;
            return 0;
        }

        case BLKDISCARD: {
            uint64_t *range = va_arg(args, uint64_t *);
            const dhara_sector_t begin = range[0] / DHARA_SECTOR_SIZE;
            const dhara_sector_t end = (range[0] + range[1]) / DHARA_SECTOR_SIZE;
            for (dhara_sector_t s = begin; s < end; s++) {
                dhara_error_t err;
                if (dhara_map_trim(&file->map, s, &err) < 0) {
                    return dhara_check_ret(-1, err);
                }
            }
            return dhara_check_ret(0, DHARA_E_NONE);
        }

        default: {
            errno = EINVAL;
            return -1;
        }
    }
}

static off_t dhara_lseek(void *ctx, off_t pos, int whence) {
    struct dhara_file *file = ctx;
    off_t new_pos;
    switch (whence) {
        case SEEK_SET:
            new_pos = 0;
            break;
        case SEEK_CUR:
            new_pos = file->pos;
            break;
        case SEEK_END:
            new_pos = file->limit;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    new_pos += pos / DHARA_SECTOR_SIZE;
    if (new_pos < 0) {
        errno = EINVAL;
        return -1;
    }
    if (new_pos > file->limit) {
        errno = EFBIG;
        return -1;
    }
    file->pos = new_pos;
    return new_pos;
}

static int dhara_read(void *ctx, void *buf, size_t size) {
    struct dhara_file *file = ctx;
    size_t br = 0;
    const dhara_sector_t end = MIN(file->pos + size / DHARA_SECTOR_SIZE, file->limit);
    while (file->pos < end) {
        dhara_error_t err;
        if (dhara_map_read(&file->map, file->pos, buf + br, &err) < 0) {
            return dhara_check_ret(-1, err);
        }
        file->pos++;
        br += DHARA_SECTOR_SIZE;
    }
    return br;
}

static int dhara_write(void *ctx, const void *buf, size_t size) {
    struct dhara_file *file = ctx;
    size_t bw = 0;
    const dhara_sector_t end = file->pos + size / DHARA_SECTOR_SIZE;
    if (end > file->limit) {
        errno = EFBIG;
        return -1;
    }
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        errno = EROFS;
        return -1;
    }
    while (file->pos < end) {
        dhara_error_t err;
        if (dhara_map_write(&file->map, file->pos, buf + bw, &err) < 0) {
            return dhara_check_ret(-1, err);
        }
        file->pos++;
        bw += DHARA_SECTOR_SIZE;
    }
    return bw;
}

static const struct vfs_file_vtable dhara_vtable = {
    .close = dhara_close,
    .ioctl = dhara_ioctl,
    .lseek = dhara_lseek,
    .read = dhara_read,
    .write = dhara_write,
};

void *dhara_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    struct dhara_file *file = malloc(sizeof(struct dhara_file) + 0);
    if (!file) {
        errno = ENOMEM;
        return NULL;
    }

    vfs_file_init(&file->base, &dhara_vtable, mode);

    file->nor.log2_page_size = __builtin_ctz(DHARA_SECTOR_SIZE);
    file->nor.log2_ppb = __builtin_ctz(FLASH_SECTOR_SIZE / DHARA_SECTOR_SIZE);
    file->nor.num_blocks = (__flash_storage_end - __flash_storage_start) / FLASH_SECTOR_SIZE;

    dhara_map_init(&file->map, &file->nor, file->page_buf, DHARA_GC_RATIO);

    dhara_error_t err;
    if (dhara_map_resume(&file->map, &err) < 0) {
        fputs("dhara map not found\n", stderr);
    } else if (flags & O_TRUNC) {
        dhara_map_clear(&file->map);
    }

    file->pos = 0;
    file->limit = dhara_map_capacity(&file->map);

    file->flags = flags;
    return file;
}

int dhara_nand_is_bad(const struct dhara_nand *n, dhara_block_t b) {
    return 0;
}

void dhara_nand_mark_bad(const struct dhara_nand *n, dhara_block_t b) {
}

int dhara_nand_erase(const struct dhara_nand *n, dhara_block_t b, dhara_error_t *err) {
    const uint32_t flash_offs = __flash_storage_start - (uint8_t *)XIP_BASE;
    flash_lockout_start();
    flash_range_erase(flash_offs + b * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
    flash_lockout_end();
    return 0;
}

int dhara_nand_prog(const struct dhara_nand *n, dhara_page_t p, const uint8_t *data, dhara_error_t *err) {
    const uint32_t flash_offs = __flash_storage_start - (uint8_t *)XIP_BASE;
    flash_lockout_start();
    flash_range_program(flash_offs + p * DHARA_SECTOR_SIZE, data, DHARA_SECTOR_SIZE);
    flash_lockout_end();
    return 0;
}

int dhara_nand_is_free(const struct dhara_nand *n, dhara_page_t p) {
    const uint32_t *ptr = (uint32_t *)(__flash_storage_start + p * DHARA_SECTOR_SIZE);
    for (size_t i = 0; i < DHARA_SECTOR_SIZE / sizeof(uint32_t); i++) {
        if (~ptr[i]) {
            return 0;
        }
    }
    return 1;
}

int dhara_nand_read(const struct dhara_nand *n, dhara_page_t p, size_t offset, size_t length, uint8_t *data, dhara_error_t *err) {
    vTaskSuspendAll();
    memcpy(data, __flash_storage_start + p * DHARA_SECTOR_SIZE + offset, length);
    xTaskResumeAll();
    return 0;
}

int dhara_nand_copy(const struct dhara_nand *n, dhara_page_t src, dhara_page_t dst, dhara_error_t *err) {
    uint8_t data[DHARA_SECTOR_SIZE];
    const uint32_t flash_offs = __flash_storage_start - (uint8_t *)XIP_BASE;
    flash_lockout_start();
    memcpy(data, __flash_storage_start + src * DHARA_SECTOR_SIZE, DHARA_SECTOR_SIZE);
    flash_range_program(flash_offs + dst * DHARA_SECTOR_SIZE, data, DHARA_SECTOR_SIZE);
    flash_lockout_end();
    return 0;
}
