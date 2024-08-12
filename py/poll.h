// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "FreeRTOS.h"
#include "task.h"

#include "py/runtime.h"

enum mp_poll_ctl_op {
    MP_POLL_CTL_ADD,
    MP_POLL_CTL_MOD,
    MP_POLL_CTL_DEL,
};

typedef struct {
    void (*signal)(mp_obj_t poll_obj, mp_obj_t stream_obj, mp_uint_t events, BaseType_t *pxHigherPriorityTaskWoken);
} mp_poll_p_t;

typedef struct {
    mp_obj_t poll_obj;
    enum mp_poll_ctl_op op;
    mp_obj_t stream_obj;
    mp_uint_t event_mask;
} mp_poll_ctl_ioctl_args_t;

mp_uint_t mp_poll_ctl(mp_obj_t poll_obj, enum mp_poll_ctl_op op, mp_obj_t stream_obj, mp_uint_t event_mask, int *errcode);

// ###

typedef struct {
    mp_obj_base_t base;
    TaskHandle_t task;
    nlr_jump_callback_node_t nlr_callback;
    mp_obj_t stream_obj;
} mp_obj_poll_t;

void mp_poll_init(mp_obj_poll_t *self, const mp_obj_type_t *type, mp_obj_t stream_obj, mp_uint_t event_mask);

bool mp_poll_wait(mp_obj_poll_t *self, TickType_t *timeout);

void mp_poll_deinit(mp_obj_poll_t *self);

mp_uint_t mp_poll_block(mp_obj_t stream_obj, void *buf, mp_uint_t size, int *errcode, mp_uint_t (*func)(mp_obj_t, void *, mp_uint_t, int *), mp_uint_t events, TickType_t xTicksToWait, bool greedy);

uint32_t mp_ulTaskNotifyTake(BaseType_t xClearCountOnExit, TickType_t *pxTicksToWait);

void mp_vTaskDelay(TickType_t xTicksToDelay);
