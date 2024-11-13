// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/types.h>


struct devfs_entry {
    const char *path;
    mode_t mode;
    dev_t dev;
};

extern const struct vfs_filesystem devfs_fs;
extern const struct devfs_entry devfs_entries[];
extern const size_t devfs_num_entries;
