// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>
#include <reent.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/reent.h>

#include "newlib/newlib.h"
#include "freertos/task_helper.h"
#include "timers.h"


struct fd_entry {
    const struct fd_vtable *func;
    void *state;
    int flags;
};

static struct fd_entry fd_table;

static struct fd_entry *fd_lookup(struct _reent *ptr, int fd) {
    if ((fd < 3) && fd_table.func) {
        return &fd_table;
    }
    __errno_r(ptr) = EBADF;
    return NULL;
}

int fd_open(const struct fd_vtable *func, void *state, int flags) {
    if (fd_table.func) {
        return -1;
    }

    struct fd_entry *file = &fd_table;
    file->func = func;
    file->state = state;
    file->flags = flags;
    return 0;
}

int fd_close(void) {
    if (!fd_table.func) {
        return -1;
    }
    struct fd_entry *file = &fd_table;
    int ret = file->func->close(file->state);
    file->func = NULL;
    file->state = NULL;
    file->flags = 0;
    return ret;
}

int _close_r(struct _reent *ptr, int fd) {
    struct fd_entry *file = fd_lookup(ptr, fd);
    if (!file) {
        return -1;
    }
    __errno_r(ptr) = EPERM;
    return -1;
}

int _fstat_r(struct _reent *ptr, int fd, struct stat *pstat) {
    struct fd_entry *file = fd_lookup(ptr, fd);
    if (!file) {
        return -1;
    }
    pstat->st_mode = S_IFCHR;
    return 0;
}

int _getpid_r(struct _reent *ptr) {
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    TaskStatus_t task_status;
    vTaskGetInfo(task, &task_status, pdFALSE, eRunning);
    return task_status.xTaskNumber;
}

int _isatty_r(struct _reent *ptr, int fd) {
    struct fd_entry *file = fd_lookup(ptr, fd);
    if (!file) {
        return -1;
    }
    return 1;
}

int __sigtramp(int sig);

int _kill_r(struct _reent *ptr, int pid, int sig) {
    if (sig != SIGINT) {
        __errno_r(ptr) = EINVAL;
        return -1;
    }

    UBaseType_t num_tasks;
    TaskStatus_t *task_status_array = NULL;
    while (1) {
        num_tasks = uxTaskGetNumberOfTasks();
        task_status_array = malloc(num_tasks * sizeof(TaskStatus_t));
        if (!task_status_array) {
            __errno_r(ptr) = ENOMEM;
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
        struct _reent *other_ptr = task_get_reent(task);
        assert(_impure_ptr == ptr);
        _impure_ptr = other_ptr;
        __sigtramp(sig);
        _impure_ptr = ptr;
        if (task != xTaskGetCurrentTaskHandle()) {
            vTaskResume(task);
        }
        num_signals++;
    }
    free(task_status_array);

    if (num_signals == 0) {
        __errno_r(ptr) = ESRCH;
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

off_t _lseek_r(struct _reent *ptr, int fd, off_t pos, int whence) {
    struct fd_entry *file = fd_lookup(ptr, fd);
    if (!file) {
        return -1;
    }
    __errno_r(ptr) = ESPIPE;
    return -1;
}

_ssize_t _read_r(struct _reent *ptr, int fd, void *buf, size_t cnt) {
    struct fd_entry *file = fd_lookup(ptr, fd);
    if (!file) {
        return -1;
    }
    return file->func->read(file->state, buf, cnt);
}

_ssize_t _write_r(struct _reent *ptr, int fd, const void *buf, size_t cnt) {
    struct fd_entry *file = fd_lookup(ptr, fd);
    if (!file) {
        return -1;
    }
    return file->func->write(file->state, buf, cnt);
}
