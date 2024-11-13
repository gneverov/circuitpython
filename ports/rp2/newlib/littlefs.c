// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <sys/unistd.h>

#include "newlib/ioctl.h"
#include "newlib/mtd.h"
#include "newlib/vfs.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "lfs.h"


static int littlefs_result(lfs_ssize_t result) {
    if (result >= 0) {
        return result;
    }
    errno = -result;
    return -1;
}

struct littlefs_mount {
    struct vfs_mount base;
    #ifdef LFS_THREADSAFE
    SemaphoreHandle_t mutex;
    #endif
    int fd;
    lfs_t lfs;
    struct lfs_config config;
    struct lfs_fsinfo fsinfo;
    #ifdef LFS_THREADSAFE
    StaticSemaphore_t xMutexBuffer;
    #endif
};

struct littlefs_file {
    struct vfs_file base;
    struct littlefs_mount *vfs;
    lfs_file_t file;
    off_t pos;
};

struct littlefs_dir {
    struct vfs_file base;
    struct littlefs_mount *vfs;
    lfs_dir_t dir;
    struct lfs_info info;
    struct dirent dirent;
};

static const struct vfs_mount_vtable littlefs_vtable;

static const struct vfs_file_vtable littlefs_file_vtable;

static const struct vfs_file_vtable littlefs_dir_vtable;

static int littlefs_block_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size) {
    struct littlefs_mount *vfs = c->context;
    return (pread(vfs->fd, buffer, size, block * c->block_size + off) >= 0) ? LFS_ERR_OK : -errno;
}

static int littlefs_block_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size) {
    struct littlefs_mount *vfs = c->context;
    return (pwrite(vfs->fd, buffer, size, block * c->block_size + off) >= 0) ? LFS_ERR_OK : -errno;
}

static int littlefs_block_erase(const struct lfs_config *c, lfs_block_t block) {
    struct littlefs_mount *vfs = c->context;
    struct erase_info erase_info = {
        block *c->block_size,
        c->block_size,
    };
    return (ioctl(vfs->fd, MEMERASE, &erase_info) >= 0) ? LFS_ERR_OK : -errno;
}

static int littlefs_block_sync(const struct lfs_config *c) {
    struct littlefs_mount *vfs = c->context;
    return (fsync(vfs->fd) >= 0) ? LFS_ERR_OK : -errno;
}

#ifdef LFS_THREADSAFE
static int littlefs_block_lock(const struct lfs_config *c) {
    struct littlefs_mount *vfs = c->context;
    xSemaphoreTake(vfs->mutex, portMAX_DELAY);
    return LFS_ERR_OK;
}

static int littlefs_block_unlock(const struct lfs_config *c) {
    struct littlefs_mount *vfs = c->context;
    xSemaphoreGive(vfs->mutex);
    return LFS_ERR_OK;
}
#endif

static struct littlefs_mount *littlefs_init(const char *source, int flags) {
    struct littlefs_mount *vfs = calloc(1, sizeof(struct littlefs_mount));
    if (!vfs) {
        return NULL;
    }
    vfs_mount_init(&vfs->base, &littlefs_vtable);
    #ifdef LFS_THREADSAFE
    vfs->mutex = xSemaphoreCreateMutexStatic(&vfs->xMutexBuffer);
    #endif
    int fd = open(source, flags);
    if (fd < 0) {
        return NULL;
    }
    vfs->fd = fd;
    struct mtd_info mtd_info;
    if (ioctl(fd, MEMGETINFO, &mtd_info) < 0) {
        return NULL;
    }
    vfs->config = (struct lfs_config) {
        .context = vfs,
        .read = littlefs_block_read,
        .prog = littlefs_block_prog,
        .erase = littlefs_block_erase,
        .sync = littlefs_block_sync,
        .read_size = 1,
        .prog_size = mtd_info.writesize,
        .block_size = mtd_info.erasesize,
        .block_count = mtd_info.size / mtd_info.erasesize,
        .block_cycles = 1000,
        .cache_size = mtd_info.erasesize,
        .lookahead_size = 8,
    };
    return vfs;
}

static void littlefs_deinit(struct littlefs_mount *vfs) {
    if (vfs->fd >= 0) {
        close(vfs->fd);
    }
    vfs->fd = -1;
    #ifdef LFS_THREADSAFE
    if (vfs->mutex) {
        vSemaphoreDelete(vfs->mutex);
    }
    vfs->mutex = NULL;
    #endif
}

static int littlefs_mkfs(const void *ctx, const char *source, const char *data) {
    struct littlefs_mount *vfs = littlefs_init(source, O_RDWR | O_TRUNC);
    if (!vfs) {
        return -1;
    }
    int result = littlefs_result(lfs_format(&vfs->lfs, &vfs->config));
    littlefs_deinit(vfs);
    return result;
}

static void *littlefs_mount(const void *ctx, const char *source, unsigned long mountflags, const char *data) {
    struct littlefs_mount *vfs = littlefs_init(source, O_RDWR | O_TRUNC);
    if (!vfs) {
        return NULL;
    }
    if (littlefs_result(lfs_mount(&vfs->lfs, &vfs->config)) < 0) {
        littlefs_deinit(vfs);
        vfs = NULL;
    }
    if (littlefs_result(lfs_fs_stat(&vfs->lfs, &vfs->fsinfo)) < 0) {
        littlefs_deinit(vfs);
        vfs = NULL;
    }
    return vfs;
}

const struct vfs_filesystem littlefs_fs = {
    .type = "littlefs",
    .mkfs = littlefs_mkfs,
    .mount = littlefs_mount,
};

static int littlefs_mkdir(void *ctx, const char *path, mode_t mode) {
    struct littlefs_mount *vfs = ctx;
    return littlefs_result(lfs_mkdir(&vfs->lfs, path));
}

static void *littlefs_open(void *ctx, const char *path, int flags, mode_t mode) {
    struct littlefs_mount *vfs = ctx;
    int lfs_flags = 0;
    switch (flags & O_ACCMODE) {
        case O_RDONLY:
            lfs_flags = LFS_O_RDONLY;
            break;
        case O_WRONLY:
            lfs_flags = LFS_O_WRONLY;
            break;
        case O_RDWR:
            lfs_flags = LFS_O_RDWR;
            break;
    }
    if (flags & O_CREAT) {
        lfs_flags |= LFS_O_CREAT;
    }
    if (flags & O_EXCL) {
        lfs_flags |= LFS_O_EXCL;
    }
    if (flags & O_TRUNC) {
        lfs_flags |= LFS_O_TRUNC;
    }
    if (flags & O_APPEND) {
        lfs_flags |= LFS_O_APPEND;
    }
    if (flags & ~(O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND)) {
        errno = EINVAL;
        return NULL;
    }

    struct littlefs_file *file = malloc(sizeof(struct littlefs_file));
    vfs_file_init(&file->base, &littlefs_file_vtable, (mode & ~S_IFMT) | S_IFREG);
    file->vfs = vfs;
    if (littlefs_result(lfs_file_open(&vfs->lfs, &file->file, path, lfs_flags)) < 0) {
        free(file);
        file = NULL;
    }
    return file;
}

static void *littlefs_opendir(void *ctx, const char *dirname) {
    struct littlefs_mount *vfs = ctx;
    struct littlefs_dir *dir = malloc(sizeof(struct littlefs_dir));
    vfs_file_init(&dir->base, &littlefs_dir_vtable, S_IFDIR);
    dir->vfs = vfs;
    if (littlefs_result(lfs_dir_open(&vfs->lfs, &dir->dir, dirname)) < 0) {
        free(dir);
        dir = NULL;
    }
    return dir;
}

static int littlefs_rename(void *ctx, const char *old, const char *new) {
    struct littlefs_mount *vfs = ctx;
    return littlefs_result(lfs_rename(&vfs->lfs, old, new));
}

static void littlefs_init_stat(struct littlefs_mount *vfs, const struct lfs_info *info, struct stat *pstat) {
    if (info->type == LFS_TYPE_REG) {
        pstat->st_mode = S_IFREG;
    } else if (info->type == LFS_TYPE_DIR) {
        pstat->st_mode = S_IFDIR;
    }
    pstat->st_size = info->size;
    pstat->st_blksize = vfs->fsinfo.block_size;
}

static int littlefs_stat(void *ctx, const char *path, struct stat *pstat) {
    struct littlefs_mount *vfs = ctx;
    struct lfs_info info;
    if (littlefs_result(lfs_stat(&vfs->lfs, path, &info)) < 0) {
        return -1;
    }
    littlefs_init_stat(vfs, &info, pstat);
    return 0;
}

static int littlefs_syncfs(void *ctx) {
    struct littlefs_mount *vfs = ctx;
    return littlefs_result(littlefs_block_sync(&vfs->config));
}

static int littlefs_statvfs(void *ctx, struct statvfs *buf) {
    struct littlefs_mount *vfs = ctx;
    lfs_ssize_t lfs_size = lfs_fs_size(&vfs->lfs);
    if (littlefs_result(lfs_size) < 0) {
        return -1;
    }

    buf->f_bsize = vfs->fsinfo.block_size;
    buf->f_frsize = buf->f_bsize;
    buf->f_blocks = vfs->fsinfo.block_count;
    buf->f_bfree = buf->f_blocks - lfs_size;
    buf->f_bavail = buf->f_bfree;
    buf->f_files = 0;
    buf->f_ffree = 0;
    buf->f_favail = 0;
    buf->f_fsid = 0;
    buf->f_flag = 0;
    buf->f_namemax = vfs->fsinfo.name_max;
    return 0;
}

static int littlefs_umount(void *ctx) {
    struct littlefs_mount *vfs = ctx;
    int result = littlefs_result(lfs_unmount(&vfs->lfs));
    littlefs_deinit(vfs);
    free(vfs);
    return result;
}

static int littlefs_unlink(void *ctx, const char *path) {
    struct littlefs_mount *vfs = ctx;
    return littlefs_result(lfs_remove(&vfs->lfs, path));
}

static const struct vfs_mount_vtable littlefs_vtable = {
    .mkdir = littlefs_mkdir,
    .open = littlefs_open,
    .rename = littlefs_rename,
    .stat = littlefs_stat,
    .umount = littlefs_umount,
    .unlink = littlefs_unlink,

    .opendir = littlefs_opendir,
    .rmdir = littlefs_unlink,

    .statvfs = littlefs_statvfs,
    .syncfs = littlefs_syncfs,
};


static int littlefs_close(void *ctx) {
    struct littlefs_file *file = ctx;
    int result = littlefs_result(lfs_file_close(&file->vfs->lfs, &file->file));
    free(file);
    return result;
}

static int littlefs_closedir(void *ctx) {
    struct littlefs_dir *dir = ctx;
    int result = littlefs_result(lfs_dir_close(&dir->vfs->lfs, &dir->dir));
    free(dir);
    return result;
}

static int littlefs_fstat(void *ctx, struct stat *pstat) {
    struct littlefs_file *file = ctx;
    lfs_soff_t lfs_size = littlefs_result(lfs_file_size(&file->vfs->lfs, &file->file));
    if (lfs_size < 0) {
        return -1;
    }
    struct lfs_info info = {
        .type = LFS_TYPE_REG,
        .size = lfs_size,
    };
    littlefs_init_stat(file->vfs, &info, pstat);
    return 0;
}

static off_t littlefs_lseek(void *ctx, off_t offset, int whence) {
    static_assert(LFS_SEEK_SET == SEEK_SET);
    static_assert(LFS_SEEK_CUR == SEEK_CUR);
    static_assert(LFS_SEEK_END == SEEK_END);
    struct littlefs_file *file = ctx;
    int result = littlefs_result(lfs_file_seek(&file->vfs->lfs, &file->file, offset, whence));
    if (result < 0) {
        return -1;
    }
    return file->pos = result;
}

static int littlefs_pread(void *ctx, void *buffer, size_t size, off_t offset) {
    struct littlefs_file *file = ctx;
    if (littlefs_result(lfs_file_seek(&file->vfs->lfs, &file->file, offset, SEEK_SET)) < 0) {
        return -1;
    }
    return littlefs_result(lfs_file_read(&file->vfs->lfs, &file->file, buffer, size));
}

static int littlefs_read(void *ctx, void *buffer, size_t size) {
    struct littlefs_file *file = ctx;
    int result = littlefs_pread(file, buffer, size, file->pos);
    file->pos = lfs_file_tell(&file->vfs->lfs, &file->file);
    return result;
}

static struct dirent *littlefs_readdir(void *ctx) {
    struct littlefs_dir *dir = ctx;
    do {
        if (littlefs_result(lfs_dir_read(&dir->vfs->lfs, &dir->dir, &dir->info)) <= 0) {
            return NULL;
        }
    }
    while ((strcmp(dir->info.name, ".") == 0) || (strcmp(dir->info.name, "..") == 0));
    dir->dirent.d_ino = 0;
    dir->dirent.d_type = (dir->info.type == LFS_TYPE_DIR) ? DT_DIR : DT_REG;
    dir->dirent.d_name = dir->info.name;
    return strlen(dir->dirent.d_name) ? &dir->dirent : NULL;
}

static void littlefs_rewinddir(void *ctx) {
    struct littlefs_dir *dir = ctx;
    littlefs_result(lfs_dir_rewind(&dir->vfs->lfs, &dir->dir));
}

static int littlefs_pwrite(void *ctx, const void *buffer, size_t size, off_t offset) {
    struct littlefs_file *file = ctx;
    if (littlefs_result(lfs_file_seek(&file->vfs->lfs, &file->file, offset, SEEK_SET)) < 0) {
        return -1;
    }
    return littlefs_result(lfs_file_write(&file->vfs->lfs, &file->file, buffer, size));
}

static int littlefs_write(void *ctx, const void *buffer, size_t size) {
    struct littlefs_file *file = ctx;
    int result = littlefs_pwrite(file, buffer, size, file->pos);
    file->pos = lfs_file_tell(&file->vfs->lfs, &file->file);
    return result;
}

static const struct vfs_file_vtable littlefs_file_vtable = {
    .close = littlefs_close,
    .fstat = littlefs_fstat,
    .lseek = littlefs_lseek,
    .pread = littlefs_pread,
    .pwrite = littlefs_pwrite,
    .read = littlefs_read,
    .write = littlefs_write,
};

static const struct vfs_file_vtable littlefs_dir_vtable = {
    .close = littlefs_closedir,
    .readdir = littlefs_readdir,
    .rewinddir = littlefs_rewinddir,
};
