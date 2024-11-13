// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "newlib/dev.h"
#include "newlib/vfs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#ifndef MTDBLK_NUM_CACHE_ENTRIES
#define MTDBLK_NUM_CACHE_ENTRIES 16
#endif

#ifndef MTDBLK_NUM_DEVICES
#define MTDBLK_NUM_DEVICES 4
#endif


struct mtdblk_cache_entry {
    size_t num;
    void *page;
    uint tick;
};

struct mtdblk_file {
    struct vfs_file base;
    int index;
    int fd;
    size_t size;
    size_t block_size;
    SemaphoreHandle_t mutex;
    size_t pos;
    int ro;

    struct mtdblk_cache_entry cache[MTDBLK_NUM_CACHE_ENTRIES];
    uint next_tick;

    struct timespec mtime;
    StaticSemaphore_t xMutexBuffer;
};

extern const struct dev_driver mtdblk_drv;
