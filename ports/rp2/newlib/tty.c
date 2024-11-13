// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>

#include "newlib/dev.h"
#include "newlib/tty.h"
#include "newlib/vfs.h"


static void *tty_open(const void *ctx, dev_t dev, int flags, mode_t mode) {
    if (dev != DEV_TTY) {
        errno = ENODEV;
        return NULL;
    }
    struct vfs_file *file = vfs_gettty();
    if (!file) {
        errno = ENODEV;
        return NULL;
    }
    return file;
}

const struct dev_driver tty_drv = {
    .dev = DEV_TTY,
    .open = tty_open,
};
