// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "newlib/ioctl.h"

#include "hardware/flash.h"
#include "mtd.h"
#include "flash.h"
#include "flash_lockout.h"


static struct mtd_device mtd_devs[MTD_NUM_DEVICES];

static struct mtd_partition mtd_partitions[MTD_NUM_PARTITIONS];

static struct mtd_file *mtd_files[MTD_NUM_PARTITIONS];

__attribute__((constructor, visibility("hidden")))
void mtd_init(void) {
    int j = 0;
    mtd_flash_probe(&mtd_devs[0]);
    if (mtd_devs[0].info.type) {
        // check env var for partition configuration
        char *mtd_part = getenv("MTDPART");
        if (mtd_part) {
            char buf[256];
            mtd_part = strncpy(buf, mtd_part, 256);
            char *size_str = strtok(mtd_part, ",");
            uint32_t offset = 0;
            while (size_str) {
                char *end;
                size_t size = strtoul(size_str, &end, 0);
                if (*end || (offset >= mtd_devs[0].info.size) || (j >= MTD_NUM_PARTITIONS)) {
                    break;
                }
                size = MIN(size, mtd_devs[0].info.size - offset);
                mtd_partitions[j].device = &mtd_devs[0];
                mtd_partitions[j].offset = offset;
                mtd_partitions[j].size = size;
                offset += size;
                j++;
                size_str = strtok(NULL, ",");
            }
        }
        // if no partitions were setup, set default config
        if (j == 0) {
            // storage partition 3/4 of flash size
            mtd_partitions[0].device = &mtd_devs[0];
            mtd_partitions[0].offset = 0;
            mtd_partitions[0].size = 3 * mtd_devs[0].info.size / 4;
            // storage partition: 1/4 of flash size
            mtd_partitions[1].device = &mtd_devs[0];
            mtd_partitions[1].offset = mtd_partitions[0].size;
            mtd_partitions[1].size = mtd_devs[0].info.size / 4;
            j = 2;
        }
    }

    #if !PICO_RP2040
    mtd_psram_probe(&mtd_devs[1]);
    if (mtd_devs[1].info.type) {
        // default single partition
        mtd_partitions[j].device = &mtd_devs[1];
        mtd_partitions[j].offset = 0;
        mtd_partitions[j].size = mtd_devs[1].info.size;
        j++;
    }
    #endif
}

static int mtd_close(void *ctx) {
    struct mtd_file *file = ctx;
    dev_lock();
    assert(mtd_files[file->index] == file);
    mtd_files[file->index] = NULL;
    dev_unlock();
    vSemaphoreDelete(file->mutex);
    free(file);
    return 0;
}

static int mtd_fstat(void *ctx, struct stat *pstat) {
    struct mtd_file *file = ctx;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    pstat->st_mtim = file->mtime;
    xSemaphoreGive(file->mutex);
    return 0;
}

static int mtd_ioctl(void *ctx, unsigned long request, va_list args) {
    struct mtd_file *file = ctx;
    const struct mtd_partition *part = &mtd_partitions[file->index];
    int ret = -1;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    switch (request) {
        case MEMGETINFO: {
            struct mtd_info *info = va_arg(args, struct mtd_info *);
            *info = part->device->info;
            info->size = part->size;
            ret = 0;
            break;
        }

        case MEMERASE: {
            struct erase_info *info = va_arg(args, struct erase_info *);
            flash_lockout_start();
            flash_range_erase(part->offset + (info->start & -FLASH_SECTOR_SIZE), info->length & -FLASH_SECTOR_SIZE);
            flash_lockout_end();
            ret = 0;
            break;
        }

        default: {
            errno = EINVAL;
            break;
        }
    }
    xSemaphoreGive(file->mutex);
    return ret;
}

static off_t mtd_lseek(void *ctx, off_t pos, int whence) {
    struct mtd_file *file = ctx;
    const struct mtd_partition *part = &mtd_partitions[file->index];
    int ret = -1;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    switch (whence) {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            pos += file->pos;
            break;
        case SEEK_END:
            pos += part->size;
            break;
        default:
            errno = EINVAL;
            goto exit;
    }
    if (pos < 0) {
        errno = EINVAL;
        goto exit;
    }
    if (pos > part->size) {
        errno = EFBIG;
        goto exit;
    }
    ret = file->pos = pos;
exit:
    xSemaphoreGive(file->mutex);
    return ret;
}

static void *mtd_mmap(void *ctx, void *addr, size_t len, int prot, int flags, off_t off) {
    struct mtd_file *file = ctx;
    const struct mtd_partition *part = &mtd_partitions[file->index];
    if ((off >= part->size) || (off + len > part->size)) {
        errno = ENXIO;
        return NULL;
    }
    if ((prot & PROT_WRITE) && (part->device->info.type != MTD_RAM)) {
        errno = ENOTSUP;
        return NULL;
    }
    return (void *)(part->device->mmap_addr + part->offset + off);
}

static int mtd_rread(struct mtd_file *file, void *buffer, size_t size, off_t *offset) {
    const struct mtd_partition *part = &mtd_partitions[file->index];
    size = MIN(size, part->size - *offset);
    memcpy(buffer, (void *)(part->offset + *offset + part->device->rw_addr), size);
    *offset += size;
    return size;
}

static int mtd_pread(void *ctx, void *buffer, size_t size, off_t offset) {
    struct mtd_file *file = ctx;
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    int result = mtd_rread(file, buffer, size, &offset);
    xSemaphoreGive(file->mutex);
    return result;
}

static int mtd_rwrite(struct mtd_file *file, const void *buffer, size_t size, off_t *offset) {
    const struct mtd_partition *part = &mtd_partitions[file->index];
    if (*offset + size > part->size) {
        errno = EFBIG;
        return -1;
    }
    struct timeval t;
    gettimeofday(&t, NULL);
    if (part->device->info.erasesize > 1) {
        *offset &= -FLASH_PAGE_SIZE;
        size &= -FLASH_PAGE_SIZE;
        flash_lockout_start();
        flash_range_program(part->offset + *offset, buffer, size);
        flash_lockout_end();
    } else {
        memcpy((void *)(part->offset + *offset + part->device->rw_addr), buffer, size);
    }
    *offset += size;
    file->mtime.tv_sec = t.tv_sec;
    file->mtime.tv_nsec = t.tv_usec * 1000;
    return size;
}

static int mtd_pwrite(void *ctx, const void *buffer, size_t size, off_t offset) {
    struct mtd_file *file = ctx;
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    int result = mtd_rwrite(file, buffer, size, &offset);
    xSemaphoreGive(file->mutex);
    return result;
}

static int mtd_read(void *ctx, void *buffer, size_t size) {
    struct mtd_file *file = ctx;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    int ret = mtd_pread(file, buffer, size, file->pos);
    xSemaphoreGive(file->mutex);
    return ret;
}

static int mtd_write(void *ctx, const void *buffer, size_t size) {
    struct mtd_file *file = ctx;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    int ret = mtd_pwrite(file, buffer, size, file->pos);
    xSemaphoreGive(file->mutex);
    return ret;
}

static const struct vfs_file_vtable mtd_vtable = {
    .close = mtd_close,
    .fstat = mtd_fstat,
    .ioctl = mtd_ioctl,
    .lseek = mtd_lseek,
    .mmap = mtd_mmap,
    .pread = mtd_pread,
    .pwrite = mtd_pwrite,
    .read = mtd_read,
    .write = mtd_write,
};

static void *mtd_open(const void *ctx, dev_t dev, int flags, mode_t mode) {
    int index = minor(dev);
    if ((index >= MTD_NUM_PARTITIONS) || !mtd_partitions[index].device) {
        errno = ENODEV;
        return NULL;
    }

    dev_lock();
    struct mtd_file *file = mtd_files[index];
    if (file) {
        vfs_copy_file(&file->base);
        goto exit;
    }
    file = calloc(1, sizeof(struct mtd_file));
    if (!file) {
        goto exit;
    }
    vfs_file_init(&file->base, &mtd_vtable, mode | S_IFCHR);
    file->index = index;
    file->mutex = xSemaphoreCreateMutexStatic(&file->xMutexBuffer);
    mtd_files[index] = file;

exit:
    dev_unlock();
    return file;
}

const struct dev_driver mtd_drv = {
    .dev = DEV_MTD0,
    .open = mtd_open,
};
