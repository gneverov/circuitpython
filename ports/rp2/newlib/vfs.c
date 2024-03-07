// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "freertos/task_helper.h"
#include "newlib/newlib.h"
#include "newlib/vfs.h"

#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t vfs_mutex;

static struct vfs_mount *vfs_table;

static struct vfs_file *vfs_fd_table[VFS_FD_MAX];

__attribute__((constructor))
void vfs_init(void) {
    vfs_mutex = xSemaphoreCreateMutex();
}

#ifndef NDEBUG
static bool vfs_is_locked(void) {
    return xSemaphoreGetMutexHolder(vfs_mutex) == xTaskGetCurrentTaskHandle();
}
#endif

static void vfs_lock(void) {
    assert((xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) || !vfs_is_locked());
    xSemaphoreTake(vfs_mutex, portMAX_DELAY);

}
static void vfs_unlock(void) {
    assert((xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) || vfs_is_locked());
    xSemaphoreGive(vfs_mutex);
}

char *vfs_getcwd(void) {
    return pvTaskGetThreadLocalStoragePointer(NULL, TLS_INDEX_CWD);
}

void vfs_setcwd(char *value) {
    vTaskSetThreadLocalStoragePointer(NULL, TLS_INDEX_CWD, value);
}

static const struct vfs_filesystem *vfs_lookup_filesystem(const char *type) {
    const struct vfs_filesystem *fs = NULL;
    for (int i = 0; i < vfs_num_fss; i++) {
        if (strcmp(vfs_fss[i]->type, type) == 0) {
            fs = vfs_fss[i];
            break;
        }
    }
    return fs;
}

void vfs_mount_init(struct vfs_mount *vfs, const struct vfs_vtable *func) {
    vfs->func = func;
    vfs->ref_count = 1;
    vfs->path = NULL;
    vfs->path_len = 0;
    vfs->next = NULL;
}

// A valid path must:
// - begin with '/'
// - not end with '/', unless the whole path is "/"

// Compares whether path1 is a prefix of path2.
// Returns NULL if path1 is not a prefix of path2.
// Otherwise return the remainder of path2, after the path1 prefix is removed.
char *vfs_compare_path(const char *path1, const char *path2) {
    size_t len = strlen(path1);
    if (strncmp(path1, path2, len) != 0) {
        return NULL;
    }
    path2 += path1[1] ? len : 0;
    if ((path2[0] == '/') || (path2[0] == '\0')) {
        return (char *)path2;
    }
    return NULL;
}

// Expand path replacing any "." or ".." elements, prepending the CWD, and create a proper absolute path.
int vfs_expand_path(vfs_path_buffer_t *vfs_path, const char *path) {
    const char *limit = vfs_path->buf + sizeof(vfs_path->buf) - 1;
    // The output path does not begin at the beginning of the buffer to allow consumers to prepend their own data.
    char *out = vfs_path->begin = vfs_path->buf + 2;
    *out = '\0';

    if (*path == '/') {
        // absolute path
        path++;
    } else if (*path != '\0') {
        // relative path
        const char *cwd = vfs_getcwd();
        if (cwd && (strlen(cwd) > 1)) {
            out = stpcpy(out, cwd);
        }
    } else {
        // empty path is invalid
        errno = ENOENT;
        return -1;
    }

    char *next;
    do {
        next = strchrnul(path, '/');
        size_t len = next - path;
        if ((len == 0) || (strncmp(path, ".", len) == 0)) {
        } else if (strncmp(path, "..", len) == 0) {
            char *prev = strrchr(vfs_path->begin, '/');
            out = prev ? prev : vfs_path->begin;
            *out = '\0';
        } else if (out + len < limit) {
            out = stpcpy(out, "/");
            out = stpncpy(out, path, len);
            *out = '\0';
        } else {
            errno = ENAMETOOLONG;
            return -1;
        }
        path = next + 1;
    }
    while (*next);
    if (*out == '\0') {
        strcpy(out, "/");
    }
    return 0;
}

bool vfs_iterate_mount(struct vfs_mount **entry) {
    vfs_lock();
    if (*entry) {
        *entry = (*entry)->next;
    } else {
        *entry = vfs_table;
    }
    if (*entry) {
        (*entry)->ref_count++;
    }
    vfs_unlock();
    return *entry;
}

struct vfs_mount *vfs_acquire_mount(const char *file, vfs_path_buffer_t *vfs_path) {
    if (vfs_expand_path(vfs_path, file)) {
        return NULL;
    }

    vfs_lock();
    struct vfs_mount *entry = vfs_table;
    while (entry) {
        char *next = vfs_compare_path(entry->path, vfs_path->begin);
        if (next != NULL) {
            if (*next == '\0') {
                strcpy(next, "/");
            }
            vfs_path->begin = next;
            entry->ref_count++;
            goto exit;
        }
        entry = entry->next;
    }
    errno = ENOENT;
exit:
    vfs_unlock();
    return entry;
}

void vfs_release_mount(struct vfs_mount *vfs) {
    vfs_lock();
    int ref_count = --vfs->ref_count;
    vfs_unlock();
    if (ref_count == 0) {
        char *path = vfs->path;
        if (vfs->func->umount) {
            vfs->func->umount(vfs);
        }
        free(path);
    }
}

static bool vfs_mount_lookup(const char *path, struct vfs_mount ***vfs) {
    assert((xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) || vfs_is_locked());
    size_t path_len = strlen(path);

    *vfs = &vfs_table;
    while (**vfs) {
        struct vfs_mount *entry = **vfs;
        if (strcmp(entry->path, path) == 0) {
            return true;
        }
        if (entry->path_len < path_len) {
            break;
        }
        *vfs = &entry->next;
    }
    return false;
}

int mkfs(const char *source, const char *filesystemtype, const char *data) {
    const struct vfs_filesystem *fs = vfs_lookup_filesystem(filesystemtype);
    if (!fs) {
        errno = ENODEV;
        return -1;
    }
    if (fs->mkfs) {
        return fs->mkfs(fs, source, data);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const char *data) {
    const struct vfs_filesystem *fs = vfs_lookup_filesystem(filesystemtype);
    if (!fs) {
        errno = ENODEV;
        return -1;
    }

    vfs_path_buffer_t vfs_path;
    if (vfs_expand_path(&vfs_path, target) < 0) {
        return -1;
    }
    if (!fs->mount) {
        errno = ENOSYS;
        return -1;
    }
    struct vfs_mount *mount = fs->mount(fs, source, mountflags, data);
    if (!mount) {
        return -1;
    }

    int ret = -1;
    size_t path_len = strlen(vfs_path.begin);
    mount->path = malloc(path_len + 1);
    mount->path_len = path_len;
    if (!mount->path) {
        errno = ENOMEM;
        goto cleanup;
    }
    strcpy(mount->path, vfs_path.begin);

    vfs_lock();
    struct vfs_mount **pentry;
    if (vfs_mount_lookup(mount->path, &pentry)) {
        errno = EEXIST;
    } else {
        mount->next = *pentry;
        *pentry = mount;
        mount = NULL;
        ret = 0;
    }
    vfs_unlock();

cleanup:
    if (mount) {
        vfs_release_mount(mount);
    }
    return ret;
}

int umount(const char *path) {
    vfs_lock();
    int ret = -1;
    struct vfs_mount **pentry;
    struct vfs_mount *vfs = NULL;
    if (vfs_mount_lookup(path, &pentry)) {
        vfs = *pentry;
        *pentry = vfs->next;
        vfs->next = NULL;
    } else {
        errno = EINVAL;
    }
    vfs_unlock();
    if (vfs) {
        vfs_release_mount(vfs);
        ret = 0;
    }
    return ret;
}

void vfs_file_init(struct vfs_file *file, const struct vfs_file_vtable *func, mode_t mode) {
    file->func = func;
    file->ref_count = 1;
    file->mode = mode;
}

struct vfs_file *vfs_acquire_file(int fd) {
    if ((uint)fd >= VFS_FD_MAX) {
        errno = EBADF;
        return NULL;
    }

    vfs_lock();
    struct vfs_file *file = vfs_fd_table[fd];
    if (file) {
        file->ref_count++;
    } else {
        errno = EBADF;
    }
    vfs_unlock();
    return file;
}

struct vfs_file *vfs_copy_file(struct vfs_file *file) {
    vfs_lock();
    file->ref_count++;
    vfs_unlock();
    return file;
}

void vfs_release_file(struct vfs_file *file) {
    vfs_lock();
    int ref_count = --file->ref_count;
    vfs_unlock();
    if ((ref_count == 0) && (file->func->close)) {
        file->func->close(file);
    }
}

static int vfs_fd_next(void) {
    assert((xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) || vfs_is_locked());
    for (int fd = 3; fd < VFS_FD_MAX; fd++) {
        if (!vfs_fd_table[fd]) {
            return fd;
        }
    }
    errno = ENFILE;
    return -1;
}

int vfs_close(int fd) {
    if ((uint)fd >= VFS_FD_MAX) {
        errno = EBADF;
        return -1;
    }

    vfs_lock();
    int ret = -1;
    struct vfs_file *file = vfs_fd_table[fd];
    if (!file) {
        errno = EBADF;
        goto exit;
    }
    vfs_fd_table[fd] = NULL;
exit:
    vfs_unlock();
    if (file) {
        vfs_release_file(file);
        ret = 0;
    }
    return ret;
}

int vfs_replace(int fd, struct vfs_file *file) {
    if (fd >= VFS_FD_MAX) {
        errno = EBADF;
        return -1;
    }

    vfs_lock();
    struct vfs_file *prev_file = NULL;
    if (fd < 0) {
        fd = vfs_fd_next();
    }
    if (fd < 0) {
        goto exit;
    }
    file->ref_count++;
    prev_file = vfs_fd_table[fd];
    vfs_fd_table[fd] = file;
exit:
    vfs_unlock();
    if (prev_file) {
        vfs_release_file(prev_file);
    }
    return fd;
}

int dup(int oldfd) {
    struct vfs_file *old_file = vfs_acquire_file(oldfd);
    if (!old_file) {
        return -1;
    }
    int ret = vfs_replace(-1, old_file);
    vfs_release_file(old_file);
    return ret;
}

int dup2(int oldfd, int newfd) {
    if ((uint)newfd >= VFS_FD_MAX) {
        errno = EBADF;
        return -1;
    }
    struct vfs_file *old_file = vfs_acquire_file(oldfd);
    if (!old_file) {
        return -1;
    }
    int ret = vfs_replace(newfd, old_file);
    vfs_release_file(old_file);
    return ret;
}
