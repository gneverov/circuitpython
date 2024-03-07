// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include "newlib/mount.h"
#include "newlib/statvfs.h"

#define VFS_FD_MAX 8

struct vfs_filesystem {
    const char *type;
    int (*mkfs)(const void *ctx, const char *source, const char *data);
    void *(*mount)(const void *ctx, const char *source, unsigned long mountflags, const char *data);
};

struct vfs_vtable {
    int (*mkdir)(void *ctx, const char *path, mode_t mode);
    void *(*open)(void *ctx, const char *file, int flags, mode_t mode);
    int (*rename)(void *ctx, const char *old, const char *new);
    int (*stat)(void *ctx, const char *file, struct stat *pstat);
    int (*unlink)(void *ctx, const char *file);

    void *(*opendir)(void *ctx, const char *dirname);
    int (*rmdir)(void *ctx, const char *path);

    int (*statvfs)(void *ctx, struct statvfs *buf);
    int (*syncfs)(void *ctx);
    int (*truncate)(void *ctx, const char *path, off_t length);

    int (*umount)(void *ctx);
};

struct vfs_file_vtable {
    int (*close)(void *ctx);
    int (*fstat)(void *ctx, struct stat *pstat);
    int isatty;
    off_t (*lseek)(void *ctx, off_t pos, int whence);
    int (*read)(void *ctx, void *buf, size_t nbyte);
    int (*write)(void *ctx, const void *buf, size_t nbyte);

    struct dirent *(*readdir)(void *ctx);
    void (*rewinddir)(void *ctx);

    int (*fstatvfs)(void *ctx, struct statvfs *buf);
    int (*fsync)(void *ctx);
    int (*ftruncate)(void *ctx, off_t length);

    int (*ioctl)(void *ctx, unsigned long request, va_list args);
};

struct vfs_mount {
    const struct vfs_vtable *func;
    int ref_count;
    char *path;
    size_t path_len;
    struct vfs_mount *next;
};

struct vfs_file {
    const struct vfs_file_vtable *func;
    int ref_count;
    mode_t mode;
};

typedef struct {
    char *begin;
    char buf[256];
} vfs_path_buffer_t;

extern const struct vfs_filesystem *vfs_fss[];
extern const size_t vfs_num_fss;

inline char *strchrnul(const char *str, char ch) {
    char *ret = strchr(str, ch);
    return ret ? ret : (char *)str + strlen(str);
}

char *vfs_compare_path(const char *path1, const char *path2);

int vfs_expand_path(vfs_path_buffer_t *vfs_path, const char *path);

void vfs_mount_init(struct vfs_mount *vfs, const struct vfs_vtable *func);

bool vfs_iterate_mount(struct vfs_mount **entry);

struct vfs_mount *vfs_acquire_mount(const char *file, vfs_path_buffer_t *vfs_path);

void vfs_release_mount(struct vfs_mount *vfs);

int vfs_mount(struct vfs_mount *vfs, int mountflags);

void vfs_file_init(struct vfs_file *file, const struct vfs_file_vtable *func, mode_t mode);

struct vfs_file *vfs_acquire_file(int fd);

void vfs_release_file(struct vfs_file *file);

int vfs_replace(int fd, struct vfs_file *file);

int vfs_close(int fd);

struct vfs_file *vfs_fd(struct _reent *ptr, int fd);

char *vfs_getcwd(void);

void vfs_setcwd(char *value);
