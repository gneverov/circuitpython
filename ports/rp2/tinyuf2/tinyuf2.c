// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

#include "newlib/dlfcn.h"
#include "newlib/flash_heap.h"
#include "newlib/ioctl.h"
#include "newlib/newlib.h"
#include "newlib/vfs.h"

#include "uf2.h"
#include "tinyuf2/tinyuf2.h"


static struct tinyuf2_file {
    struct vfs_file base;
    off_t ptr;
    WriteState wr_state;
    dl_loader_t loader;
} *tinyuf2_file;

static int tinyuf2_close(void *ctx) {
    struct tinyuf2_file *file = ctx;
    dl_loader_free(&file->loader);
    free(file);
    if (tinyuf2_file == file) {
        tinyuf2_file = NULL;
    }
    return 0;
}

static int tinyuf2_ioctl(void *ctx, unsigned long request, va_list args) {
    struct tinyuf2_file *file = ctx;
    switch (request) {
        case BLKROSET: {
            return 0;
        }

        case BLKROGET: {
            int *ro = va_arg(args, int *);
            *ro = file->wr_state.aborted;
            return 0;
        }

        case BLKGETSIZE: {
            unsigned long *size = va_arg(args, unsigned long *);
            *size = CFG_UF2_NUM_BLOCKS;
            return 0;
        }

        case BLKFLSBUF: {
            return 0;
        }

        case BLKSSZGET: {
            int *ssize = va_arg(args, int *);
            *ssize = 512;
            return 0;
        }

        default: {
            errno = EINVAL;
            return -1;
        }
    }
}

static off_t tinyuf2_lseek(void *ctx, off_t pos, int whence) {
    struct tinyuf2_file *file = ctx;
    off_t ptr;
    switch (whence) {
        case SEEK_SET:
            ptr = 0;
            break;
        case SEEK_CUR:
            ptr = file->ptr;
            break;
        case SEEK_END:
            ptr = CFG_UF2_NUM_BLOCKS * 512;
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
    if (ptr > CFG_UF2_NUM_BLOCKS * 512) {
        errno = EFBIG;
        return -1;
    }
    file->ptr = (ptr + 511) & ~511;
    return file->ptr;
}

static int tinyuf2_read(void *ctx, void *buf, size_t size) {
    struct tinyuf2_file *file = ctx;
    uint32_t lba = file->ptr / 512;
    uint8_t *buffer = buf;
    uint32_t count = 0;
    while (count < size) {
        uf2_read_block(lba, buffer);

        lba++;
        buffer += 512;
        count += 512;
        file->ptr += 512;
    }
    return count;
}

static int tinyuf2_write(void *ctx, const void *buf, size_t size) {
    struct tinyuf2_file *file = ctx;
    uint32_t lba = file->ptr / 512;
    uint8_t *buffer = (void *)buf;
    uint32_t count = 0;
    while (count < size) {
        // Consider non-uf2 block write as successful
        // only break if write_block is busy with flashing (return 0)
        if (0 == uf2_write_block(lba, buffer, &file->wr_state)) {
            break;
        }

        lba++;
        buffer += 512;
        count += 512;
        file->ptr += 512;
    }
    return count;
}

static const struct vfs_file_vtable tinyuf2_vtable = {
    .close = tinyuf2_close,
    .ioctl = tinyuf2_ioctl,
    .lseek = tinyuf2_lseek,
    .read = tinyuf2_read,
    .write = tinyuf2_write,
};

void *tinyuf2_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    if (tinyuf2_file) {
        errno = EBUSY;
        return NULL;
    }
    struct tinyuf2_file *file = malloc(sizeof(struct tinyuf2_file));
    if (!file) {
        errno = ENOMEM;
        return NULL;
    }
    vfs_file_init(&file->base, &tinyuf2_vtable, mode);
    file->ptr = 0;
    memset(&file->wr_state, 0, sizeof(file->wr_state));
    if (dl_loader_open(&file->loader, FLASH_BASE) < 0) {
        vfs_release_file(&file->base);
        return NULL;
    }
    uf2_init();
    tinyuf2_file = file;
    return file;
}

static void tinyuf2_link(void *pvParameters) {
    struct tinyuf2_file *file = pvParameters;
    int ret = dl_link(&file->loader);
    vfs_release_file(&file->base);
    if (ret >= 0) {
        exit(0);
    }
    vTaskDelete(NULL);
}


uint32_t board_flash_size(void) {
    return PICO_FLASH_SIZE_BYTES;
}

void board_flash_read(uint32_t addr, void *buffer, uint32_t len) {
    if (tinyuf2_file) {
        dl_loader_read(&tinyuf2_file->loader, buffer, len, addr);
    }
}

void board_flash_flush(void) {
    struct tinyuf2_file *file = tinyuf2_file;
    if (file && !file->wr_state.aborted) {
        file->wr_state.aborted = 1;

        // Because the flash heap cache can use all available RAM, we need to evict some cache so
        // that there is enough free RAM to allocate the FreeRTOS task. So we ensure there is at
        // least double the task's stack size of free RAM before creating the task.
        void *alloc = flash_heap_realloc_with_evict(&file->loader.heap, NULL, 1024 * sizeof(StackType_t));
        free(alloc);

        // Increment ref counter on file object for the reference used by the new task.
        file = (struct tinyuf2_file *)vfs_copy_file(&file->base);
        if (xTaskCreate(tinyuf2_link, "uf2", 512, file, 1, NULL) != pdPASS) {
            vfs_release_file(&file->base);
        }
    }
}

void board_flash_write(uint32_t addr, void const *data, uint32_t len) {
    struct tinyuf2_file *file = tinyuf2_file;
    if (file && !file->wr_state.aborted) {
        if (dl_loader_write(&file->loader, data, len, addr) < 0) {
            // If there is a write error (probably an out of space error) we abort the flashing.
            file->wr_state.aborted = 1;
        }
    }
}
