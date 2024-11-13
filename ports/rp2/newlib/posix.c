// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

#include "newlib/dev.h"
#include "newlib/thread.h"
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
    vfs_setcwd(vfs_path.begin);
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

char *ctermid(char *s) {
    char *ret = "/dev/tty";
    return s ? strcpy(s, ret) : ret;
}

DIR *fdopendir(int fd) {
    int flags = 0;
    struct vfs_file *file = vfs_acquire_file(fd, &flags);
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
    int flags = 0;
    struct vfs_file *file = vfs_acquire_file(fd, &flags);
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
    int flags = 0;
    struct vfs_file *file = vfs_acquire_file(fd, &flags);
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
    int flags = FWRITE;
    struct vfs_file *file = vfs_acquire_file(fd, &flags);
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
    vfs_getcwd(buf, size);
    return buf;
}

int getentropy(void *buffer, size_t length) {
    return getrandom(buffer, length, 0) >= length ? 0 : -1;
}

int gethostname(char *name, size_t namelen) {
    const char *hostname = getenv("HOSTNAME");
    if (!hostname || !namelen) {
        errno = EINVAL;
        return -1;
    }
    strncpy(name, hostname, namelen);
    return 0;
}

__attribute__((visibility("hidden")))
int vioctl(int fd, unsigned long request, va_list args) {
    int flags = 0;
    struct vfs_file *file = vfs_acquire_file(fd, &flags);
    if (!file) {
        return -1;
    }
    int ret = -1;
    if (file->func->ioctl) {
        ret = file->func->ioctl(file, request, args);
    } else {
        errno = ENOTTY;
    }
    vfs_release_file(file);
    return ret;
}

int ioctl(int fd, unsigned long request, ...) {
    va_list args;
    va_start(args, request);
    int ret = vioctl(fd, request, args);
    va_end(args);
    return ret;
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp) {
    int ret = 0;
    TickType_t xTicksToWait = rqtp->tv_sec * configTICK_RATE_HZ + (rqtp->tv_nsec * configTICK_RATE_HZ + 999999999) / 1000000000;
    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    while (!xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait)) {
        if (thread_enable_interrupt()) {
            ret = -1;
            break;
        }
        vTaskDelay(xTicksToWait);
        thread_disable_interrupt();
    }
    if (rmtp) {
        rmtp->tv_sec = xTicksToWait / configTICK_RATE_HZ;
        rmtp->tv_nsec = xTicksToWait % configTICK_RATE_HZ * (1000000000 / configTICK_RATE_HZ);
    }
    return ret;
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

char *realpath(const char *file_name, char *resolved_name) {
    vfs_path_buffer_t vfs_path;
    if (vfs_expand_path(&vfs_path, file_name) < 0) {
        return NULL;
    }
    return resolved_name ? strcpy(resolved_name, vfs_path.begin) : strdup(vfs_path.begin);
}

unsigned sleep(unsigned seconds) {
    struct timespec rqtp = {.tv_sec = seconds, .tv_nsec = 0 };
    struct timespec rmtp;
    nanosleep(&rqtp, &rmtp);
    return rmtp.tv_sec;
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

#ifndef UTSNAME_SYSNAME
#define UTSNAME_SYSNAME "Picolibc"
#endif
#ifndef UTSNAME_RELEASE
#define UTSNAME_RELEASE __PICOLIBC_VERSION__
#endif
#ifndef UTSNAME_VERSION
#define UTSNAME_VERSION __DATE__
#endif
#ifndef UTSNAME_MACHINE
#if defined(__ARM_ARCH)
#define UTSNAME_MACHINE ("armv" __XSTRING(__ARM_ARCH))
#else
#error unsupported machine
#endif
#endif

int uname(struct utsname *name) {
    strncpy(name->sysname, UTSNAME_SYSNAME, UTSNAME_LENGTH);
    #ifdef UTSNAME_NODENAME
    strncpy(name->nodename, UTSNAME_NODENAME, UTSNAME_LENGTH);
    #else
    if (gethostname(name->nodename, UTSNAME_LENGTH) < 0) {
        strncpy(name->nodename, "", UTSNAME_LENGTH);
    }
    #endif
    strncpy(name->release, UTSNAME_RELEASE, UTSNAME_LENGTH);
    strncpy(name->version, UTSNAME_VERSION, UTSNAME_LENGTH);
    strncpy(name->machine, UTSNAME_MACHINE, UTSNAME_LENGTH);
    return 0;
}

void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    int fd_flags = 0;
    struct vfs_file *file = vfs_acquire_file(fd, &fd_flags);
    if (!file) {
        return NULL;
    }
    if (!(fd_flags & FREAD)) {
        errno = EACCES;
        return NULL;
    }
    if (!(fd_flags & FWRITE) && (prot & PROT_WRITE)) {
        errno = EACCES;
        return NULL;
    }
    if (!len || (off < 0)) {
        errno = EINVAL;
        return NULL;
    }
    void *ret = NULL;
    if (file->func->mmap) {
        ret = file->func->mmap(file, addr, len, prot, flags, off);
    } else {
        errno = ENODEV;
    }
    vfs_release_file(file);
    return ret;
}

int munmap(void *addr, size_t len) {
    return 0;
}


#include <signal.h>
#include "timers.h"
static TimerHandle_t alarm_timer;

static void alarm_callback(TimerHandle_t xTimer) {
    dev_lock();
    assert(alarm_timer == xTimer);
    alarm_timer = NULL;
    dev_unlock();
    xTimerDelete(xTimer, portMAX_DELAY);
    raise(SIGALRM);
}

unsigned alarm(unsigned seconds) {
    TickType_t xTimerPeriod = pdMS_TO_TICKS(1000u * seconds);
    unsigned result = 0;
    dev_lock();
    if (alarm_timer) {
        TickType_t xExpiryTime = xTimerGetExpiryTime(alarm_timer);
        xTimerChangePeriod(alarm_timer, xTimerPeriod, portMAX_DELAY);
        result = (xExpiryTime - xTaskGetTickCount()) / configTICK_RATE_HZ;
    } else {
        alarm_timer = xTimerCreate("alarm", xTimerPeriod, pdFALSE, NULL, alarm_callback);
        xTimerStart(alarm_timer, portMAX_DELAY);
    }
    dev_unlock();
    return result;
}
