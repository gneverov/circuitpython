// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>

#include "newlib/dev.h"
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
    const struct devfs_entry *entry;
    size_t index;
    struct dirent dirent;
};

static const struct vfs_file_vtable devfs_dir_vtable;

static const struct devfs_entry *devfs_lookup(void *ctx, const char *path) {
    for (int i = 0; i < devfs_num_entries; i++) {
        const struct devfs_entry *entry = &devfs_entries[i];
        if (strcmp(entry->path, path) == 0) {
            return entry;
        }
    }
    errno = ENOENT;
    return NULL;
}

static void *devfs_open(void *ctx, const char *path, int flags, mode_t mode) {
    const struct devfs_entry *entry = devfs_lookup(ctx, path);
    if (!entry) {
        return NULL;
    }
    if (S_ISDIR(entry->mode)) {
        errno = EISDIR;
        return NULL;
    }
    int fd = opendev(entry->dev, flags, entry->mode);
    if (fd < 0) {
        return NULL;
    }
    struct vfs_file *file = vfs_acquire_file(fd, &flags);
    close(fd);
    return file;
}

static void *devfs_opendir(void *ctx, const char *dirname) {
    const struct devfs_entry *entry = devfs_lookup(ctx, dirname);
    if (!entry) {
        return NULL;
    }
    if (!S_ISDIR(entry->mode)) {
        errno = ENOTDIR;
        return NULL;
    }
    if (entry->dev) {
        int fd = opendev(entry->dev, O_SEARCH, entry->mode);
        if (fd < 0) {
            return NULL;
        }
        return fdopendir(fd);
    }
    struct devfs_dir *dir = malloc(sizeof(struct devfs_dir));
    if (!dir) {
        return NULL;
    }
    vfs_file_init(&dir->base, &devfs_dir_vtable, entry->mode);
    dir->entry = entry;
    dir->index = 0;
    return dir;
}

static int devfs_stat(void *ctx, const char *file, struct stat *pstat) {
    const struct devfs_entry *entry = devfs_lookup(ctx, file);
    if (!entry) {
        return -1;
    }
    pstat->st_mode = entry->mode;
    pstat->st_rdev = entry->dev;
    return 0;
}

static int devfs_umount(void *ctx) {
    struct vfs_mount *vfs = ctx;
    vfs->path = NULL;
    return 0;
}

static const struct vfs_mount_vtable devfs_vtable = {
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
    while (dir->index < devfs_num_entries) {
        const struct devfs_entry *entry = &devfs_entries[dir->index++];
        char *next = vfs_compare_path(dir->entry->path, entry->path);
        if ((next != NULL) && (strlen(next) > 1) && (strchr(next + 1, '/') == NULL)) {
            dir->dirent.d_ino = 0;
            dir->dirent.d_type = entry->mode & S_IFMT;
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
