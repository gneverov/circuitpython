// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>
#include <reent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/unistd.h>

#include "newlib/dirent.h"
#include "newlib/vfs.h"


int chdir(const char *path) {
    vfs_path_buffer_t vfs_path;
    struct vfs_mount *vfs = vfs_acquire_mount(path, &vfs_path);
    if (!vfs) {
        return -1;
    }
    int ret = -1;
    struct stat buf = { .st_mode = S_IFDIR };
    if (strcmp(vfs_path.begin, "/") == 0) {
        // Root is always a valid directory and filesystem may not support stat of root directory.
        ret = 0;
    } else if (vfs->func->stat) {
        ret = vfs->func->stat(vfs, vfs_path.begin, &buf);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);

    if (ret < 0) {
        return -1;
    }
    if (!S_ISDIR(buf.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    // Re-expand path since the call to stat may have modified it.
    if (vfs_expand_path(&vfs_path, path) < 0) {
        return -1;
    }
    size_t len = strlen(vfs_path.begin);
    char *cwd = vfs_getcwd();
    cwd = realloc(cwd, len + 1);
    strcpy(cwd, vfs_path.begin);
    vfs_setcwd(cwd);
    return 0;
}

int closedir(DIR *dirp) {
    struct vfs_file *file = dirp;
    if (!file) {
        return -1;
    }
    if (S_ISDIR(file->mode)) {
        vfs_release_file(file);
        return 0;

    } else {
        errno = ENOTDIR;
        return -1;
    }
}

DIR *fdopendir(int fd) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return NULL;
    }
    if (S_ISDIR(file->mode)) {
        return (DIR *)file;
    } else {
        vfs_release_file(file);
        errno = ENOTDIR;
        return NULL;
    }
}

int fstatvfs(int fd, struct statvfs *buf) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = -1;
    if (file->func->fstatvfs) {
        memset(buf, 0, sizeof(struct statvfs));
        ret = file->func->fstatvfs(file, buf);
    } else {
        errno = ENOSYS;
    }
    vfs_release_file(file);
    return ret;
}

int fsync(int fd) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = 0;
    if (file->func->fsync) {
        ret = file->func->fsync(file);
    }
    vfs_release_file(file);
    return ret;
}

int ftruncate(int fd, off_t length) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = -1;
    if (file->func->ftruncate) {
        ret = file->func->ftruncate(file, length);
    } else {
        errno = ENOSYS;
    }
    vfs_release_file(file);
    return ret;
}

char *getcwd(char *buf, size_t size) {
    const char *cwd = vfs_getcwd();
    return strncpy(buf, cwd ? cwd : "/", size);
}

int ioctl(int fd, unsigned long request, ...) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = -1;
    if (file->func->ioctl) {
        va_list args;
        va_start(args, request);
        ret = file->func->ioctl(file, request, args);
        va_end(args);
    } else {
        errno = ENOSYS;
    }
    vfs_release_file(file);
    return ret;
}

int mkdir(const char *path, mode_t mode) {
    return _mkdir_r(_impure_ptr, path, mode);
}

DIR *opendir(const char *dirname) {
    vfs_path_buffer_t vfs_dirname;
    struct vfs_mount *vfs = vfs_acquire_mount(dirname, &vfs_dirname);
    if (!vfs) {
        return NULL;
    }
    struct vfs_file *file = NULL;
    if (vfs->func->opendir) {
        file = vfs->func->opendir(vfs, vfs_dirname.begin);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);

    return file;
}

ssize_t pread(int fd, void *buf, size_t nbyte, off_t offset) {
    if (lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    return read(fd, buf, nbyte);
}

ssize_t pwrite(int fd, const void *buf, size_t nbyte, off_t offset) {
    if (lseek(fd, offset, SEEK_SET) < 0) {
        return -1;
    }
    return write(fd, buf, nbyte);
}

struct dirent *readdir(DIR *dirp) {
    struct vfs_file *file = dirp;
    if (!file) {
        return NULL;
    }
    struct dirent *ret = NULL;
    if (!S_ISDIR(file->mode)) {
        errno = ENOTDIR;
    } else if (file->func->readdir) {
        ret = file->func->readdir(file);
    } else {
        errno = ENOSYS;
    }
    return ret;
}

void rewinddir(DIR *dirp) {
    struct vfs_file *file = dirp;
    if (!file) {
        return;
    }
    if (!S_ISDIR(file->mode)) {
        errno = ENOTDIR;
    }
    if (file->func->rewinddir) {
        file->func->rewinddir(dirp);
    } else {
        errno = ENOSYS;
    }
}

int rmdir(const char *path) {
    vfs_path_buffer_t vfs_path;
    struct vfs_mount *vfs = vfs_acquire_mount(path, &vfs_path);
    if (!vfs) {
        return -1;
    }
    int ret = -1;
    if (vfs->func->rmdir) {
        ret = vfs->func->rmdir(vfs, vfs_path.begin);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);
    return ret;
}

int statvfs(const char *path, struct statvfs *buf) {
    vfs_path_buffer_t vfs_path;
    struct vfs_mount *vfs = vfs_acquire_mount(path, &vfs_path);
    if (!vfs) {
        return -1;
    }
    int ret = -1;
    if (vfs->func->statvfs) {
        memset(buf, 0, sizeof(struct statvfs));
        ret = vfs->func->statvfs(vfs, buf);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);
    return ret;
}

void sync(void) {
    struct vfs_mount *vfs = NULL;
    while (vfs_iterate_mount(&vfs)) {
        if (vfs->func->syncfs) {
            vfs->func->syncfs(vfs);
        }
        vfs_release_mount(vfs);
    }
}

int truncate(const char *path, off_t length) {
    vfs_path_buffer_t vfs_path;
    struct vfs_mount *vfs = vfs_acquire_mount(path, &vfs_path);
    if (!vfs) {
        return -1;
    }
    int ret = -1;
    if (vfs->func->truncate) {
        ret = vfs->func->truncate(vfs, vfs_path.begin, length);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);
    return ret;
}
