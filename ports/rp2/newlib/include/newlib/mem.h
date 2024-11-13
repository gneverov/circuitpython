// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "newlib/vfs.h"


struct mem_file {
    struct vfs_file base;
    dev_t dev;
    void *ptr;
};

extern const struct dev_driver mem_drv;
