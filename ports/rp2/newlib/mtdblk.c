// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "newlib/ioctl.h"
#include "newlib/mtd.h"
#include "newlib/mtdblk.h"


static struct mtdblk_file *mtdblk_files[MTDBLK_NUM_DEVICES];

__attribute__((constructor, visibility("hidden")))
void mtdblk_init(void) {
}

static void mtdblk_file_deinit(struct mtdblk_file *file) {
    if (file->fd >= 0) {
        close(file->fd);
    }
    file->fd = -1;
    for (size_t i = 0; i < MTDBLK_NUM_CACHE_ENTRIES; i++) {
        free(file->cache[i].page);
        file->cache[i].page = NULL;
    }
    vSemaphoreDelete(file->mutex);
}

static void *mtdblk_put_page(struct mtdblk_file *file, size_t cache_index) {
    assert(cache_index < MTDBLK_NUM_CACHE_ENTRIES);
    struct mtdblk_cache_entry *entry = &file->cache[cache_index];
    assert(entry->page);
    struct erase_info erase_info = {
        entry->num * file->block_size,
        file->block_size,
    };
    if (ioctl(file->fd, MEMERASE, &erase_info) < 0) {
        return NULL;
    }
    if (pwrite(file->fd, entry->page, file->block_size, entry->num * file->block_size) < 0) {
        return NULL;
    }
    return entry->page;
}

static void *mtdblk_evict_page(struct mtdblk_file *file) {
    size_t index = 0;
    while (file->cache[index].page == NULL) {
        if (++index == MTDBLK_NUM_CACHE_ENTRIES) {
            errno = ENOMEM;
            return NULL;
        }
    }
    size_t oldest_index = index++;
    while (index < MTDBLK_NUM_CACHE_ENTRIES) {
        if ((file->cache[index].page != NULL) && (file->cache[index].tick < file->cache[oldest_index].tick)) {
            oldest_index = index;
        }
        index++;
    }

    void *cached_page = mtdblk_put_page(file, oldest_index);
    file->cache[oldest_index].page = NULL;
    return cached_page;
}

static void *mtdblk_get_page(struct mtdblk_file *file, size_t pos) {
    assert(pos < file->size);
    size_t offset = pos % file->block_size;
    size_t page_num = pos / file->block_size;
    size_t index = page_num % MTDBLK_NUM_CACHE_ENTRIES;
    struct mtdblk_cache_entry *entry = &file->cache[index];
    if (!entry->page) {
        entry->page = malloc(file->block_size);
        if (!entry->page) {
            entry->page = mtdblk_evict_page(file);
        }
        if (!entry->page) {
            return NULL;
        }
        entry->num = ~page_num;
    } else if (page_num != entry->num) {
        mtdblk_put_page(file, index);
    }
    if (page_num != entry->num) {
        if (pread(file->fd, entry->page, file->block_size, page_num * file->block_size) < 0) {
            free(entry->page);
            entry->page = NULL;
            return NULL;
        }
        entry->num = page_num;
    }
    if (entry->tick < file->next_tick) {
        entry->tick = ++file->next_tick;
    }
    return entry->page + offset;
}

static void *mtdblk_find_page(struct mtdblk_file *file, size_t pos, size_t *len) {
    assert(pos < file->size);
    size_t offset = pos % file->block_size;
    *len = file->block_size - offset;
    size_t page_num = pos / file->block_size;
    size_t cache_index = page_num % MTDBLK_NUM_CACHE_ENTRIES;
    struct mtdblk_cache_entry *entry = &file->cache[cache_index];
    return (entry->page && (entry->num == page_num)) ? entry->page + offset : NULL;
}

static int mtdblk_flush(struct mtdblk_file *file) {
    int ret = 0;
    for (size_t i = 0; i < MTDBLK_NUM_CACHE_ENTRIES; i++) {
        if (file->cache[i].page) {
            void *page = mtdblk_put_page(file, i);
            if (page) {
                file->cache[i].page = NULL;
                free(page);
            } else {
                ret = -1;
            }
        }
    }
    return fsync(file->fd) < 0 ? -1 : ret;
}

static int mtdblk_close(void *ctx) {
    struct mtdblk_file *file = ctx;
    int ret = mtdblk_flush(file);
    dev_lock();
    assert(mtdblk_files[file->index] == file);
    mtdblk_files[file->index] = NULL;
    dev_unlock();
    mtdblk_file_deinit(file);
    free(file);
    return ret;
}

static int mtdblk_fsync(void *ctx) {
    struct mtdblk_file *file = ctx;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    int ret = mtdblk_flush(file);
    xSemaphoreGive(file->mutex);
    return ret;
}

static int mtdblk_fstat(void *ctx, struct stat *pstat) {
    struct mtdblk_file *file = ctx;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    pstat->st_rdev = makedev(major(DEV_MTDBLK0), file->index);
    pstat->st_mtim = file->mtime;
    xSemaphoreGive(file->mutex);
    return 0;
}

static int mtdblk_ioctl(void *ctx, unsigned long request, va_list args) {
    struct mtdblk_file *file = ctx;
    int ret = -1;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    switch (request) {
        case BLKROSET: {
            const int *ro = va_arg(args, const int *);
            file->ro = *ro;
            ret = 0;
            break;
        }

        case BLKROGET: {
            int *ro = va_arg(args, int *);
            *ro = file->ro;
            ret = 0;
            break;
        }

        case BLKGETSIZE: {
            unsigned long *size = va_arg(args, unsigned long *);
            *size = file->size >> 9;
            ret = 0;
            break;
        }

        case BLKFLSBUF: {
            ret = mtdblk_flush(file);
            break;
        }

        case BLKSSZGET: {
            int *ssize = va_arg(args, int *);
            *ssize = 512;
            ret = 0;
            break;
        }

        case BLKDISCARD: {
            uint64_t *range = va_arg(args, uint64_t *);
            size_t begin = (range[0] + file->block_size - 1) / file->block_size;
            size_t end = (range[0] + range[1]) / file->block_size;
            for (size_t i = 0; i < MTDBLK_NUM_CACHE_ENTRIES; i++) {
                if (file->cache[i].page && (file->cache[i].num >= begin) && (file->cache[i].num < end)) {
                    file->cache[i].page = NULL;
                    free(file->cache[i].page);
                }
            }
            ret = 0;
            break;
        }

        case MEMGETINFO:
        case MEMERASE:
        case MEMWRITEOOB:
        case MEMREADOOB:
        case MEMLOCK:
        case MEMUNLOCK: {
            errno = EINVAL;
            break;
        }

        default: {
            ret = vioctl(file->fd, request, args);
            break;
        }
    }
    xSemaphoreGive(file->mutex);
    return ret;
}

static off_t mtdblk_lseek(void *ctx, off_t pos, int whence) {
    struct mtdblk_file *file = ctx;
    int ret = -1;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    switch (whence) {
        case SEEK_SET:
            break;
        case SEEK_CUR:
            pos += file->pos;
            break;
        case SEEK_END:
            pos += file->size;
            break;
        default:
            errno = EINVAL;
            goto exit;
    }
    if (pos < 0) {
        errno = EINVAL;
        goto exit;
    }
    if (pos > file->size) {
        errno = EFBIG;
        goto exit;
    }
    ret = file->pos = pos;
exit:
    xSemaphoreGive(file->mutex);
    return ret;
}

static void *mtdblk_mmap(void *ctx, void *addr, size_t len, int prot, int flags, off_t off) {
    struct mtdblk_file *file = ctx;
    return mmap(addr, len, prot, flags, file->fd, off);
}

static int mtdblk_rread(struct mtdblk_file *file, void *buffer, size_t size, off_t *offset) {
    size = MIN(size, file->size - *offset);
    size_t remaining = size;
    while (remaining > 0) {
        size_t len;
        void *page = mtdblk_find_page(file, *offset, &len);
        len = MIN(len, remaining);
        if (page) {
            memcpy(buffer, page, len);
        } else {
            int ret = pread(file->fd, buffer, len, *offset);
            if (ret < 0) {
                return -1;
            }
            len = ret;
        }
        buffer += len;
        remaining -= len;
        *offset += len;
    }
    return size;
}

static int mtdblk_pread(void *ctx, void *buffer, size_t size, off_t offset) {
    struct mtdblk_file *file = ctx;
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    int ret = mtdblk_rread(file, buffer, size, &offset);
    xSemaphoreGive(file->mutex);
    return ret;
}

static int mtdblk_read(void *ctx, void *buffer, size_t size) {
    struct mtdblk_file *file = ctx;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    off_t offset = file->pos;
    int ret = mtdblk_rread(file, buffer, size, &offset);
    file->pos = offset;
    xSemaphoreGive(file->mutex);
    return ret;
}

static int mtdblk_rwrite(struct mtdblk_file *file, const void *buffer, size_t size, off_t *offset) {
    if (*offset + size > file->size) {
        errno = EFBIG;
        return -1;
    }
    if (file->ro) {
        errno = EROFS;
        return -1;
    }

    struct timeval t;
    gettimeofday(&t, NULL);
    file->mtime.tv_sec = t.tv_sec;
    file->mtime.tv_nsec = t.tv_usec * 1000;

    size_t remaining = size;
    while (remaining > 0) {
        size_t len;
        void *page = mtdblk_find_page(file, *offset, &len);
        if (!page && (file->block_size > 1)) {
            page = mtdblk_get_page(file, *offset);
            if (!page) {
                return -1;
            }
        }
        len = MIN(len, remaining);
        if (page) {
            memcpy(page, buffer, len);
        } else {
            int ret = pwrite(file->fd, buffer, len, *offset);
            if (ret < 0) {
                return -1;
            }
            len = ret;
        }
        buffer += len;
        remaining -= len;
        *offset += len;
    }
    return size;
}

static int mtdblk_pwrite(void *ctx, const void *buffer, size_t size, off_t offset) {
    struct mtdblk_file *file = ctx;
    if (offset < 0) {
        errno = EINVAL;
        return -1;
    }
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    int ret = mtdblk_rwrite(file, buffer, size, &offset);
    xSemaphoreGive(file->mutex);
    return ret;
}

static int mtdblk_write(void *ctx, const void *buffer, size_t size) {
    struct mtdblk_file *file = ctx;
    xSemaphoreTake(file->mutex, portMAX_DELAY);
    off_t offset = file->pos;
    int ret = mtdblk_rwrite(file, buffer, size, &offset);
    file->pos = offset;
    xSemaphoreGive(file->mutex);
    return ret;
}

static const struct vfs_file_vtable mtdblk_vtable = {
    .close = mtdblk_close,
    .fstat = mtdblk_fstat,
    .fsync = mtdblk_fsync,
    .ioctl = mtdblk_ioctl,
    .lseek = mtdblk_lseek,
    .mmap = mtdblk_mmap,
    .pread = mtdblk_pread,
    .pwrite = mtdblk_pwrite,
    .read = mtdblk_read,
    .write = mtdblk_write,
};

static int mtdblk_file_init(struct mtdblk_file *file, int index, int flags, mode_t mode, dev_t device) {
    vfs_file_init(&file->base, &mtdblk_vtable, mode | S_IFBLK);
    file->index = index;
    file->mutex = xSemaphoreCreateMutexStatic(&file->xMutexBuffer);

    dev_t mtd_device = makedev(major(DEV_MTD0), minor(device));
    file->fd = opendev(mtd_device, flags, mode);
    if (file->fd < 0) {
        return -1;
    }

    struct mtd_info mtd_info;
    if (ioctl(file->fd, MEMGETINFO, &mtd_info) < 0) {
        return -1;
    }
    file->size = mtd_info.size;
    file->block_size = mtd_info.erasesize;
    return 0;
}

static void *mtdblk_open(const void *ctx, dev_t dev, int flags, mode_t mode) {
    uint index = minor(dev);
    if (index >= MTDBLK_NUM_DEVICES) {
        errno = ENODEV;
        return NULL;
    }
    dev_lock();
    struct mtdblk_file *file = mtdblk_files[index];
    if (file) {
        vfs_copy_file(&file->base);
        goto exit;
    }
    file = calloc(1, sizeof(struct mtdblk_file));
    if (!file) {
        goto exit;
    }
    if (mtdblk_file_init(file, index, flags, mode, dev) < 0) {
        mtdblk_file_deinit(file);
        free(file);
        file = NULL;
        goto exit;
    }
    mtdblk_files[index] = file;
exit:
    dev_unlock();
    return file;
}

const struct dev_driver mtdblk_drv = {
    .dev = DEV_MTDBLK0,
    .open = mtdblk_open,
};
