// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

#include "newlib/flash_heap.h"
#include "newlib/ioctl.h"


extern uint8_t __StackLimit;
extern uint8_t end;

__attribute__((section(".flash_heap")))
static flash_heap_header_t flash_heap_flash_head = { 0, 0, 0, &end, NULL };

#if PSRAM_BASE
__attribute__((section(".psram_heap")))
static flash_heap_header_t flash_heap_psram_head = { 0, 0, 0, 0, NULL };
#endif

const flash_heap_header_t *const flash_heap_head[FLASH_HEAP_NUM_DEVICES] = {
    &flash_heap_flash_head,
    #if PSRAM_BASE
    &flash_heap_psram_head,
    #endif
};

static const flash_heap_header_t *flash_heap_tail[FLASH_HEAP_NUM_DEVICES];

static const char *const flash_heap_device_names[FLASH_HEAP_NUM_DEVICES] = {
    "/dev/firmware",
    "/dev/psram",
};

static int flash_heap_dev(uint device, flash_ptr_t *base_addr, size_t *size) {
    int fd = -1;
    if (device >= FLASH_HEAP_NUM_DEVICES) {
        errno = EINVAL;
        goto error;
    }
    fd = open(flash_heap_device_names[device], O_RDWR);
    if (fd < 0) {
        goto error;
    }
    if (ioctl(fd, BLKGETSIZE, size) < 0) {
        goto error;
    }
    *size <<= 9;
    *base_addr = (flash_ptr_t)mmap(0, *size, PROT_READ, MAP_SHARED, fd, 0);
    if (!*base_addr) {
        goto error;
    }
    return fd;
error:
    if (fd >= 0) {
        close(fd);
    }
    return -1;
}

__attribute__((constructor(101), visibility("hidden")))
void flash_heap_init(void) {
    const flash_heap_header_t *header = flash_heap_head[0];
    while (header->type) {
        void *ram_base = sbrk(header->ram_size);
        if (header->ram_base != ram_base) {
            panic("flash heap corrupt");
            return;
        }
        header = ((void *)header) + header->flash_size;
    }
    flash_heap_tail[0] = header;
    flash_heap_tail[1] = flash_heap_head[1];
}

const flash_heap_header_t *flash_heap_next_header(uint device) {
    return (device < FLASH_HEAP_NUM_DEVICES) ? flash_heap_tail[device] : NULL;
}

void flash_heap_free(flash_heap_t *file) {
    if (file->fd >= 0) {
        close(file->fd);
    }
    file->fd = -1;
}

int flash_heap_open(flash_heap_t *file, uint device, uint32_t type) {
    memset(file, 0, sizeof(flash_heap_t));
    file->device = device;
    file->type = type;

    flash_ptr_t base;
    size_t size;
    file->fd = flash_heap_dev(device, &base, &size);
    if (file->fd < 0) {
        return -1;
    }

    const flash_heap_header_t *tail = flash_heap_tail[device];
    file->flash_base = base;
    file->flash_start = (flash_ptr_t)tail;
    file->flash_end = file->flash_start + sizeof(flash_heap_header_t);
    file->flash_limit = base + size;
    file->flash_pos = file->flash_end;

    file->ram_start = (flash_ptr_t)tail->ram_base;
    file->ram_end = file->ram_start;
    // file->ram_limit = (flash_ptr_t)&__StackLimit;
    return 0;
}

int flash_heap_close(flash_heap_t *file) {
    int ret = -1;
    size_t ram_size = file->ram_end - file->ram_start;
    flash_ptr_t new_tail = flash_heap_align(file->flash_end, __alignof__(flash_heap_header_t));
    flash_heap_header_t header;
    header.type = file->type;
    header.flash_size = new_tail - file->flash_start;
    header.ram_size = ram_size;
    header.ram_base = (void *)file->ram_start;
    header.entry = (void *)file->entry;
    if (flash_heap_pwrite(file, &header, sizeof(header), file->flash_start) < 0) {
        goto cleanup;
    }

    header.type = 0;
    header.flash_size = 0;
    header.ram_size = 0;
    header.ram_base = (void *)(file->ram_start + ram_size);
    header.entry = NULL;
    if (flash_heap_pwrite(file, &header, sizeof(header), new_tail) < 0) {
        goto cleanup;
    }

    if (fsync(file->fd) < 0) {
        goto cleanup;
    }
    flash_heap_tail[file->device] = (const void *)new_tail;
    ret = 0;

cleanup:
    flash_heap_free(file);
    return ret;
}

int flash_heap_seek(flash_heap_t *file, flash_ptr_t pos) {
    if (pos < file->flash_start) {
        errno = EINVAL;
        return -1;
    }
    if (pos >= file->flash_limit) {
        errno = ENOSPC;
        return -1;
    }
    file->flash_end = MAX(file->flash_end, pos);
    file->flash_pos = pos;
    return 0;
}

int flash_heap_set_ram(flash_heap_t *file, flash_ptr_t pos) {
    if (pos < file->ram_start) {
        errno = EINVAL;
        return -1;
    }
    if (pos >= file->ram_limit) {
        errno = ENOSPC;
        return -1;
    }
    file->ram_end = pos;
    return 0;
}

int flash_heap_trim(flash_heap_t *file, flash_ptr_t pos) {
    if (flash_heap_seek(file, pos) < 0) {
        return -1;
    }
    if (ftruncate(file->fd, file->flash_pos - file->flash_base) < 0) {
        return -1;
    }
    return 0;
}

flash_ptr_t flash_heap_align(flash_ptr_t addr, size_t align) {
    assert((align & (align - 1)) == 0);
    return (addr + align - 1) & ~(align - 1);
}

int flash_heap_pwrite(flash_heap_t *file, const void *buffer, size_t length, flash_ptr_t pos) {
    return pwrite(file->fd, buffer, length, pos - file->flash_base);
}

int flash_heap_pread(flash_heap_t *file, void *buffer, size_t length, flash_ptr_t pos) {
    return pread(file->fd, buffer, length, pos - file->flash_base);
}

int flash_heap_write(flash_heap_t *file, const void *buffer, size_t size) {
    int ret = flash_heap_pwrite(file, buffer, size, file->flash_pos);
    if (ret < 0) {
        return -1;
    }
    file->flash_pos += ret;
    file->flash_end = MAX(file->flash_end, file->flash_pos);
    return ret;
}

int flash_heap_read(flash_heap_t *file, void *buffer, size_t size) {
    int ret = flash_heap_pread(file, buffer, size, file->flash_pos);
    if (ret < 0) {
        return -1;
    }
    file->flash_pos += ret;
    file->flash_end = MAX(file->flash_end, file->flash_pos);
    return ret;
}

bool flash_heap_is_valid_ptr(flash_heap_t *heap, flash_ptr_t pos) {
    return
        ((pos >= heap->flash_start) && (pos <= heap->flash_end)) ||
        ((pos >= heap->ram_start) && (pos <= heap->ram_end));
}

bool flash_heap_iterate(uint device, const flash_heap_header_t **pheader) {
    if ((device >= FLASH_HEAP_NUM_DEVICES) || !flash_heap_head[device]) {
        return false;
    }
    const flash_heap_header_t *header = *pheader;
    if (header == NULL) {
        *pheader = flash_heap_head[device];
    } else {
        *pheader = ((void *)header) + header->flash_size;
    }
    return (*pheader)->type;
}

int flash_heap_truncate(uint device, const flash_heap_header_t *header) {
    if ((device >= FLASH_HEAP_NUM_DEVICES) || !flash_heap_head[device]) {
        errno = ENODEV;
        return -1;
    }
    if (!header) {
        header = flash_heap_head[device];
    }

    int ret = -1;
    flash_ptr_t base_addr;
    size_t size;
    int fd = flash_heap_dev(device, &base_addr, &size);
    if (fd < 0) {
        goto exit;
    }

    flash_heap_header_t fheader;
    off_t offset = ((uintptr_t)header) - base_addr;
    if (pread(fd, &fheader, sizeof(fheader), offset) < 0) {
        goto exit;
    }
    fheader.type = 0;
    fheader.flash_size = 0;
    fheader.ram_size = 0;
    fheader.entry = NULL;
    if (pwrite(fd, &fheader, sizeof(fheader), offset) < 0) {
        goto exit;
    }
    flash_heap_tail[device] = header;
    ret = 0;

exit:
    if (fd >= 0) {
        close(fd);
    }
    return ret;
}

void flash_heap_stats(size_t *flash_size, size_t *ram_size) {
    ram_size[0] = 0;
    for (uint i = 0; i < FLASH_HEAP_NUM_DEVICES; i++) {
        flash_size[i] = (flash_heap_tail[i] - flash_heap_head[i]) * sizeof(flash_heap_header_t);
        ram_size[0] += ((uintptr_t)flash_heap_tail[i]->ram_base) - SRAM_BASE;
    }
}
