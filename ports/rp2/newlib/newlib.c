// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <memory.h>
#include <reent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/reent.h>

#include "newlib/newlib.h"
#include "newlib/vfs.h"
#include "freertos/task_helper.h"
#include "timers.h"


int _close(int fd) {
    return vfs_close(fd);
}

int _fstat(int fd, struct stat *pstat) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = -1;
    if (file->func->fstat) {
        memset(pstat, 0, sizeof(struct stat));
        pstat->st_mode = file->mode;
        ret = file->func->fstat(file, pstat);
    } else {
        errno = ENOSYS;
    }
    vfs_release_file(file);
    return ret;
}

int _getpid(void) {
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    TaskStatus_t task_status;
    vTaskGetInfo(task, &task_status, pdFALSE, eRunning);
    return task_status.xTaskNumber;
}

int _isatty(int fd) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = file->func->isatty;
    vfs_release_file(file);
    return ret;
}

int __sigtramp(int sig);

int _kill(int pid, int sig) {
    if (sig != SIGINT) {
        errno = EINVAL;
        return -1;
    }

    UBaseType_t num_tasks;
    TaskStatus_t *task_status_array = NULL;
    while (1) {
        num_tasks = uxTaskGetNumberOfTasks();
        task_status_array = malloc(num_tasks * sizeof(TaskStatus_t));
        if (!task_status_array) {
            errno = ENOMEM;
            return -1;
        }
        vTaskSuspendAll();
        num_tasks = uxTaskGetSystemState(task_status_array, num_tasks, NULL);
        if (num_tasks == uxTaskGetNumberOfTasks()) {
            break;
        }
        xTaskResumeAll();
        free(task_status_array);
    }

    for (size_t i = 0; i < num_tasks; i++) {
        TaskStatus_t *task_status = &task_status_array[i];
        if ((pid != 0) && (pid != task_status->xTaskNumber)) {
            task_status->xHandle = NULL;
            continue;
        }
        struct _reent *other_ptr = task_get_reent(task_status->xHandle);
        if (!other_ptr) {
            task_status->xHandle = NULL;
            continue;
        }
        if (task_status->xHandle != xTaskGetCurrentTaskHandle()) {
            task_interrupt(task_status->xHandle);
            vTaskSuspend(task_status->xHandle);
        }
    }
    xTaskResumeAll();

    int num_signals = 0;
    for (size_t i = 0; i < num_tasks; i++) {
        TaskHandle_t task = task_status_array[i].xHandle;
        if (task == NULL) {
            continue;
        }
        struct _reent *ptr = _impure_ptr;
        _impure_ptr = task_get_reent(task);
        __sigtramp(sig);
        _impure_ptr = ptr;
        if (task != xTaskGetCurrentTaskHandle()) {
            vTaskResume(task);
        }
        num_signals++;
    }
    free(task_status_array);

    if (num_signals == 0) {
        errno = ESRCH;
        return -1;
    }
    return 0;
}

static void pending_kill_from_isr(void *pvParameter1, uint32_t ulParameter2) {
    int pid = (intptr_t)pvParameter1;
    int sig = ulParameter2;
    kill(pid, sig);
}

void kill_from_isr(int pid, int sig, BaseType_t *pxHigherPriorityTaskWoken) {
    BaseType_t ret = xTimerPendFunctionCallFromISR(
        pending_kill_from_isr,
        (void *)pid,
        sig,
        pxHigherPriorityTaskWoken);
    if (ret != pdPASS) {
        assert(0);
    }
}

off_t _lseek(int fd, off_t pos, int whence) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = -1;
    if (file->func->lseek) {
        ret = file->func->lseek(file, pos, whence);
    } else {
        errno = S_ISCHR(file->mode) ? ESPIPE : ENOSYS;
    }
    vfs_release_file(file);
    return ret;
}

int _mkdir(const char *path, mode_t mode) {
    vfs_path_buffer_t vfs_path;
    struct vfs_mount *vfs = vfs_acquire_mount(path, &vfs_path);
    if (!vfs) {
        return -1;
    }
    int ret = -1;
    if (vfs->func->mkdir) {
        ret = vfs->func->mkdir(vfs, vfs_path.begin, mode);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);
    return ret;
}

int _open(const char *path, int flags, mode_t mode) {
    vfs_path_buffer_t vfs_path;
    struct vfs_mount *vfs = vfs_acquire_mount(path, &vfs_path);
    if (!vfs) {
        return -1;
    }
    struct vfs_file *file = NULL;
    if (vfs->func->open) {
        file = vfs->func->open(vfs, vfs_path.begin, flags, mode);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);

    int ret = -1;
    if (file) {
        if (file->func->isatty && !(flags & O_NOCTTY)) {
            vfs_replace(0, file);
            vfs_replace(1, file);
            vfs_replace(2, file);
        }
        ret = vfs_replace(-1, file);
        vfs_release_file(file);
    }
    return ret;
}

int _read(int fd, void *buf, size_t nbyte) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = -1;
    if (file->func->read) {
        ret = file->func->read(file, buf, nbyte);
    } else {
        errno = S_ISDIR(file->mode) ? EISDIR : ENOSYS;
    }
    vfs_release_file(file);
    return ret;
}

// int _rename(const char *old, const char *new) {
int _rename_r(struct _reent *ptr, const char *old, const char *new) {
    int ret = -1;
    struct vfs_mount *vfs_old = NULL;
    struct vfs_mount *vfs_new = NULL;
    vfs_path_buffer_t vfs_path_old;
    vfs_old = vfs_acquire_mount(old, &vfs_path_old);
    if (!vfs_old) {
        goto exit;
    }
    vfs_path_buffer_t vfs_path_new;
    vfs_new = vfs_acquire_mount(new, &vfs_path_new);
    if (!vfs_new) {
        goto exit;
    }
    if (vfs_old != vfs_new) {
        errno = EXDEV;
        goto exit;
    }
    if (vfs_old->func->rename) {
        ret = vfs_old->func->rename(vfs_old, vfs_path_old.begin, vfs_path_new.begin);
    } else {
        errno = ENOSYS;
    }
exit:
    if (vfs_old) {
        vfs_release_mount(vfs_old);
    }
    if (vfs_new) {
        vfs_release_mount(vfs_new);
    }
    return ret;
}

int _stat(const char *file, struct stat *pstat) {
    vfs_path_buffer_t vfs_path;
    struct vfs_mount *vfs = vfs_acquire_mount(file, &vfs_path);
    if (!vfs) {
        return -1;
    }
    int ret = -1;
    if (vfs->func->stat) {
        memset(pstat, 0, sizeof(struct stat));
        ret = vfs->func->stat(vfs, vfs_path.begin, pstat);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);
    return ret;
}

int _unlink(const char *file) {
    vfs_path_buffer_t vfs_path;
    struct vfs_mount *vfs = vfs_acquire_mount(file, &vfs_path);
    if (!vfs) {
        return -1;
    }
    int ret = -1;
    if (vfs->func->unlink) {
        ret = vfs->func->unlink(vfs, vfs_path.begin);
    } else {
        errno = ENOSYS;
    }
    vfs_release_mount(vfs);
    return ret;
}

int _write(int fd, const void *buf, size_t nbyte) {
    struct vfs_file *file = vfs_acquire_file(fd);
    if (!file) {
        return -1;
    }
    int ret = -1;
    if (file->func->write) {
        ret = file->func->write(file, buf, nbyte);
    } else {
        errno = S_ISDIR(file->mode) ? EISDIR : ENOSYS;
    }
    vfs_release_file(file);
    return ret;
}
