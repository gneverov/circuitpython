// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include "newlib/dev.h"
#include "newlib/vfs.h"

#include "FreeRTOS.h"
#include "semphr.h"


// dev_mutex is held when a device is being opened
static SemaphoreHandle_t dev_mutex;

__attribute__((constructor, visibility("hidden")))
void dev_init(void) {
    static StaticSemaphore_t xMutexBuffer;
    dev_mutex = xSemaphoreCreateRecursiveMutexStatic(&xMutexBuffer);
}

static const struct dev_driver *finddev(dev_t dev) {
    for (size_t i = 0; i < dev_num_drvs; i++) {
        if (major(dev_drvs[i]->dev) == major(dev)) {
            return dev_drvs[i];
        }
    }
    return NULL;
}

int opendev(dev_t dev, int flags, mode_t mode) {
    const struct dev_driver *drv = finddev(dev);
    if (!drv) {
        errno = ENODEV;
        return -1;
    }
    if (!drv->open) {
        errno = ENOSYS;
        return -1;
    }
    struct vfs_file *file = drv->open(drv, dev, flags, mode & ~S_IFMT);
    if (!file) {
        return -1;
    }
    int ret = vfs_replace(-1, file, (flags & ~O_ACCMODE) | ((flags + 1) & O_ACCMODE));
    vfs_release_file(file);
    return ret;
}

int statdev(const void *ctx, dev_t dev, struct stat *pstat) {
    const struct dev_driver *drv = finddev(dev);
    if (!drv) {
        errno = ENODEV;
        return -1;
    }
    if (!drv->stat) {
        errno = ENOSYS;
        return -1;
    }
    return drv->stat(drv, dev, pstat);
}

__attribute__((visibility("hidden")))
void dev_lock(void) {
    xSemaphoreTakeRecursive(dev_mutex, portMAX_DELAY);
}

__attribute__((visibility("hidden")))
void dev_unlock(void) {
    xSemaphoreGiveRecursive(dev_mutex);
}
