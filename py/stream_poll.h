// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "FreeRTOS.h"

#include "py/poll.h"
#include "py/stream.h"

#define MP_STREAM_POLL_STD (MP_STREAM_POLL_ERR | MP_STREAM_POLL_HUP | MP_STREAM_POLL_NVAL)


typedef struct {
    mp_obj_t poll_obj;
    mp_obj_t stream_obj;
    mp_uint_t event_mask;
} mp_stream_poll_t;

void mp_stream_poll_init(mp_stream_poll_t *poll);

void mp_stream_poll_close(mp_stream_poll_t *poll);

mp_uint_t mp_stream_poll_ctl(mp_stream_poll_t *poll, const mp_poll_ctl_ioctl_args_t *args, int *errcode);

void mp_stream_poll_signal(mp_stream_poll_t *poll, mp_uint_t events, BaseType_t *pxHigherPriorityTaskWoken);

mp_uint_t mp_stream_timeout(TickType_t *timeout, mp_int_t timeout_ms, int *errcode);

mp_uint_t mp_stream_ioctl(mp_obj_t stream_obj, mp_uint_t request, uintptr_t arg, int *errcode);

mp_obj_t mp_stream_return(mp_uint_t ret, int errcode);
