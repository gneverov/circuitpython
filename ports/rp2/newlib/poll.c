// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <poll.h>
#include "newlib/thread.h"
#include "newlib/vfs.h"

#include "FreeRTOS.h"
#include "task.h"

#define POLLFILE (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM)
#define POLLCOM (POLLERR | POLLHUP | POLLNVAL)


static int poll_wait(TickType_t xTicksToWait) {
    if (thread_enable_interrupt()) {
        return -1;
    }
    uint32_t ret = ulTaskNotifyTake(pdTRUE, xTicksToWait);
    thread_disable_interrupt();
    return ret ? ret : -1;
}

struct poll_event {
    struct poll_event *next;
    TaskHandle_t task;
    // uint events;
    // uint32_t notification_value;
};

static void poll_add_waiter(struct vfs_file *file, struct poll_event *event) {
    struct poll_event **pevent = &file->event; // codespell:ignore pevent
    taskENTER_CRITICAL();
    event->next = *pevent; // codespell:ignore pevent
    *pevent = event; // codespell:ignore pevent
    taskEXIT_CRITICAL();
}

static void poll_remove_waiter(struct vfs_file *file, struct poll_event *event) {
    struct poll_event **pevent = &file->event; // codespell:ignore pevent
    taskENTER_CRITICAL();
    while (*pevent) { // codespell:ignore pevent
        if (*pevent == event) { // codespell:ignore pevent
            *pevent = event->next; // codespell:ignore pevent
            event->next = NULL;
        } else {
            pevent = &(*pevent)->next; // codespell:ignore pevent
        }
    }
    taskEXIT_CRITICAL();
}

int poll_ticks(struct pollfd fds[], nfds_t nfds, TickType_t *pxTicksToWait) {
    struct {
        struct vfs_file *file;
        struct poll_event event;
    } *array = malloc(nfds * sizeof(*array));
    if (!array) {
        return -1;
    }

    int count = 0;
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);

    for (nfds_t i = 0; i < nfds; i++) {
        int flags;
        array[i].file = vfs_acquire_file(fds[i].fd, &flags);
        array[i].event.task = task;
        if (!array[i].file) {
            fds[i].revents = POLLNVAL;
        } else if (array[i].file->func->poll) {
            poll_add_waiter(array[i].file, &array[i].event);
        } else {
            fds[i].revents = POLLFILE;
            vfs_release_file(array[i].file);
            array[i].file = NULL;
        }
    }

    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    for (;;) {
        for (nfds_t i = 0; i < nfds; i++) {
            if (array[i].file) {
                fds[i].revents = array[i].file->func->poll(array[i].file);
            }
            fds[i].revents &= fds[i].events | POLLCOM;
            if (fds[i].revents) {
                count++;
            }
        }
        if (count || xTaskCheckForTimeOut(&xTimeOut, pxTicksToWait)) {
            break;
        }
        if (poll_wait(*pxTicksToWait) < 0) {
            count = -1;
            break;
        }
    }

    for (nfds_t i = 0; i < nfds; i++) {
        if (array[i].file) {
            poll_remove_waiter(array[i].file, &array[i].event);
            vfs_release_file(array[i].file);
        }
    }

    free(array);
    return count;
}

int poll(struct pollfd fds[], nfds_t nfds, int timeout) {
    TickType_t xTicksToWait = timeout < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
    return poll_ticks(fds, nfds, &xTicksToWait);
}

void poll_notify(struct vfs_file *file, uint events) {
    struct poll_event **pevent = &file->event; // codespell:ignore pevent
    taskENTER_CRITICAL(); // codespell:ignore pevent
    while (*pevent) { // codespell:ignore pevent
        struct poll_event *event = *pevent; // codespell:ignore pevent
        // if (event->events & events) {
        xTaskNotifyGive(event->task);
        // }
        pevent = &event->next; // codespell:ignore pevent
    }
    taskEXIT_CRITICAL();
}

void poll_notify_from_isr(struct vfs_file *file, uint events, BaseType_t *pxHigherPriorityTaskWoken) {
    struct poll_event **pevent = &file->event; // codespell:ignore pevent
    UBaseType_t uxSavedInterruptStatus = taskENTER_CRITICAL_FROM_ISR();
    while (*pevent) { // codespell:ignore pevent
        struct poll_event *event = *pevent; // codespell:ignore pevent
        // if (event->events & events) {
        vTaskNotifyGiveFromISR(event->task, pxHigherPriorityTaskWoken);
        // }
        pevent = &event->next; // codespell:ignore pevent
    }
    taskEXIT_CRITICAL_FROM_ISR(uxSavedInterruptStatus);
}

int poll_file(struct vfs_file *file, uint events, int timeout) {
    struct poll_event event = {
        .next = NULL,
        .task = xTaskGetCurrentTaskHandle(),
        // .events = events,
    };
    uint revents = 0;
    int count = 0;
    ulTaskNotifyTake(pdTRUE, 0);

    if (file->func->poll) {
        poll_add_waiter(file, &event);
    } else {
        revents = POLLFILE;
        file = NULL;
    }

    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    TickType_t xTicksToWait = timeout < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
    for (;;) {
        if (file) {
            revents = file->func->poll(file);
            revents &= events | POLLCOM;
        }
        if (revents) {
            count++;
        }
        if (count || xTaskCheckForTimeOut(&xTimeOut, &xTicksToWait)) {
            break;
        }
        if (poll_wait(xTicksToWait) < 0) {
            count = -1;
            break;
        }
    }

    if (file) {
        poll_remove_waiter(file, &event);
    }
    return count;
}
