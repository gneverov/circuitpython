// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <newlib/vfs.h>


extern const struct vfs_filesystem devfs_fs;

struct devfs_driver {
    const char *path;
    mode_t mode;
    dev_t dev;
    void *(*open)(const char *fragment, int flags, mode_t mode, dev_t dev);
};

extern const struct devfs_driver devfs_drvs[];
extern const size_t devfs_num_drvs;

inline unsigned int major(dev_t dev) {
    return (dev >> 8) & 0xff;
}

inline unsigned int minor(dev_t dev) {
    return dev & 0xff;
}


enum {
    DEV_NULL = 0x0103,
    DEV_ZERO = 0x0105,
    DEV_FULL = 0x0107,
};

void *dev_open(const char *fragment, int flags, mode_t mode, dev_t dev);
