// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "py/stream_poll.h"


void mp_stream_poll_init(mp_stream_poll_t *poll) {
    poll->poll_obj = MP_OBJ_NULL;
    poll->stream_obj = MP_OBJ_NULL;
    poll->event_mask = 0;
}

void mp_stream_poll_close(mp_stream_poll_t *poll) {
    mp_stream_poll_signal(poll, MP_STREAM_POLL_NVAL, NULL);
    poll->poll_obj = MP_OBJ_NULL;
    poll->stream_obj = MP_OBJ_NULL;
}

mp_uint_t mp_stream_poll_ctl(mp_stream_poll_t *poll, const mp_poll_ctl_ioctl_args_t *args, int *errcode) {
    switch (args->op) {
        case MP_POLL_CTL_ADD:
            if (poll->poll_obj != MP_OBJ_NULL) {
                *errcode = MP_EEXIST;
                return MP_STREAM_ERROR;
            }
            poll->poll_obj = args->poll_obj;
            poll->stream_obj = args->stream_obj;
            poll->event_mask = args->event_mask;
            return 0;
        case MP_POLL_CTL_MOD:
            if (poll->poll_obj != args->poll_obj) {
                *errcode = MP_ENOENT;
                return MP_STREAM_ERROR;
            }
            poll->stream_obj = args->stream_obj;
            poll->event_mask = args->event_mask;
            return 0;
        case MP_POLL_CTL_DEL:
            if (poll->poll_obj != args->poll_obj) {
                *errcode = MP_ENOENT;
                return MP_STREAM_ERROR;
            }
            poll->poll_obj = MP_OBJ_NULL;
            poll->stream_obj = MP_OBJ_NULL;
            return 0;
        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

void mp_stream_poll_signal(mp_stream_poll_t *poll, mp_uint_t events, BaseType_t *pxHigherPriorityTaskWoken) {
    mp_obj_t poll_obj = poll->poll_obj;
    if (!poll_obj) {
        return;
    }
    events &= (poll->event_mask | MP_STREAM_POLL_STD);
    if (!events) {
        return;
    }
    const mp_obj_type_t *poll_type = mp_obj_get_type(poll_obj);
    const mp_poll_p_t *poll_p = MP_OBJ_TYPE_GET_SLOT(poll_type, protocol);
    poll_p->signal(poll_obj, poll->stream_obj, events, pxHigherPriorityTaskWoken);
}

mp_uint_t mp_stream_timeout(TickType_t *timeout, mp_int_t timeout_ms, int *errcode) {
    *timeout = timeout_ms >= 0 ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    return 0;
}

mp_uint_t mp_stream_ioctl(mp_obj_t stream_obj, mp_uint_t request, uintptr_t arg, int *errcode) {
    const mp_stream_p_t *stream_p = mp_get_stream(stream_obj);
    if (!stream_p || !stream_p->ioctl) {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }
    return stream_p->ioctl(stream_obj, request, arg, errcode);
}

mp_obj_t mp_stream_return(mp_uint_t ret, int errcode) {
    if (ret == MP_STREAM_ERROR) {
        if (mp_is_nonblocking_error(errcode)) {
            return mp_const_none;
        }
        mp_raise_OSError(errcode);
    }
    return MP_OBJ_NEW_SMALL_INT(ret);
}
