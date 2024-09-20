// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>

#include "newlib/devfs.h"
#include "newlib/vfs.h"

// Filesystem
// ----------
static struct vfs_mount devfs_global_mount;

static void *devfs_mount(const void *ctx, const char *source, unsigned long mountflags, const char *data) {
    if (devfs_global_mount.path) {
        errno = EBUSY;
        return NULL;
    }
    devfs_global_mount.ref_count++;
    return &devfs_global_mount;
}

const struct vfs_filesystem devfs_fs = {
    .type = "devfs",
    .mount = devfs_mount,
};


// Mount
// -----
struct devfs_dir {
    struct vfs_file base;
    const struct devfs_driver *dir;
    size_t index;
    struct dirent dirent;
};

static const struct vfs_file_vtable devfs_dir_vtable;

static const struct devfs_driver *devfs_lookup(void *ctx, const char *file, const char **pfragment) {
    const char *fragment = strchrnul(file, '?');
    for (int i = 0; i < devfs_num_drvs; i++) {
        const struct devfs_driver *drv = &devfs_drvs[i];
        if (strncmp(drv->path, file, fragment - file) == 0) {
            if (pfragment) {
                *pfragment = *fragment ? fragment : NULL;
            }
            return drv;
        }
    }
    errno = ENOENT;
    return NULL;
}

static void *devfs_open(void *ctx, const char *file, int flags, mode_t mode) {
    const char *fragment;
    const struct devfs_driver *drv = devfs_lookup(ctx, file, &fragment);
    if (!drv) {
        return NULL;
    }
    if (S_ISDIR(drv->mode)) {
        errno = EISDIR;
        return NULL;
    }
    return drv->open(fragment, flags, drv->mode, drv->dev);
}

static void *devfs_opendir(void *ctx, const char *dirname) {
    const struct devfs_driver *drv = devfs_lookup(ctx, dirname, NULL);
    if (!drv) {
        return NULL;
    }
    if (!S_ISDIR(drv->mode)) {
        errno = ENOTDIR;
        return NULL;
    }
    struct devfs_dir *dir = malloc(sizeof(struct devfs_dir));
    if (!dir) {
        errno = ENOMEM;
        return NULL;
    }
    vfs_file_init(&dir->base, &devfs_dir_vtable, drv->mode);
    dir->dir = drv;
    dir->index = 0;
    return dir;
}

static int devfs_stat(void *ctx, const char *file, struct stat *pstat) {
    const struct devfs_driver *drv = devfs_lookup(ctx, file, NULL);
    if (!drv) {
        return -1;
    }
    pstat->st_mode = drv->mode;
    pstat->st_rdev = drv->dev;
    return 0;
}

static int devfs_umount(void *ctx) {
    struct vfs_mount *vfs = ctx;
    vfs->path = NULL;
    return 0;
}

static const struct vfs_vtable devfs_vtable = {
    .open = devfs_open,
    .opendir = devfs_opendir,
    .stat = devfs_stat,
    .umount = devfs_umount,
};

static struct vfs_mount devfs_global_mount = {
    .func = &devfs_vtable,
};


// Dir
// ---
static int devfs_closedir(void *ctx) {
    struct devfs_dir *dir = ctx;
    free(dir);
    return 0;
}

static struct dirent *devfs_readdir(void *ctx) {
    struct devfs_dir *dir = ctx;
    while (dir->index < devfs_num_drvs) {
        const struct devfs_driver *drv = &devfs_drvs[dir->index++];
        char *next = vfs_compare_path(dir->dir->path, drv->path);
        if ((next != NULL) && (strlen(next) > 1) && (strchr(next + 1, '/') == NULL)) {
            dir->dirent.d_ino = 0;
            dir->dirent.d_type = drv->mode & S_IFMT;
            dir->dirent.d_name = (next + 1);
            return &dir->dirent;
        }
    }
    return NULL;
}

static void devfs_rewinddir(void *ctx) {
    struct devfs_dir *dir = ctx;
    dir->index = 0;
}

static const struct vfs_file_vtable devfs_dir_vtable = {
    .close = devfs_closedir,
    .readdir = devfs_readdir,
    .rewinddir = devfs_rewinddir,
};


// File
// ----
struct dev_file {
    struct vfs_file base;
    dev_t dev;
};

static const struct vfs_file_vtable dev_file_vtable;

static int dev_fstat(void *ctx, struct stat *pstat) {
    struct dev_file *file = ctx;
    pstat->st_rdev = file->dev;
    return 0;
}

void *dev_open(const char *fragment, int flags, mode_t mode, dev_t dev) {
    struct dev_file *file = malloc(sizeof(struct dev_file));
    if (!file) {
        errno = ENOMEM;
        return NULL;
    }
    vfs_file_init(&file->base, &dev_file_vtable, mode);
    file->dev = dev;
    return file;
}

static int dev_close(void *ctx) {
    struct dev_file *file = ctx;
    free(file);
    return 0;
}

static int dev_read(void *ctx, void *buf, size_t cnt) {
    struct dev_file *file = ctx;
    switch (file->dev) {
        case DEV_NULL:
            return 0;
        case DEV_ZERO:
        case DEV_FULL:
            memset(buf, 0, cnt);
            return cnt;
        default:
            errno = ENODEV;
            return -1;
    }
}

static int dev_write(void *ctx, const void *buf, size_t cnt) {
    struct dev_file *file = ctx;
    switch (file->dev) {
        case DEV_NULL:
        case DEV_ZERO:
            return cnt;
        case DEV_FULL:
            errno = ENOSPC;
            return -1;
        default:
            errno = ENODEV;
            return -1;
    }
}

static const struct vfs_file_vtable dev_file_vtable = {
    .close = dev_close,
    .fstat = dev_fstat,
    .read = dev_read,
    .write = dev_write,
};
