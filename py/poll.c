// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "freertos/task_helper.h"

#include "py/poll.h"
#include "py/stream.h"


mp_uint_t mp_poll_ctl(mp_obj_t poll_obj, enum mp_poll_ctl_op op, mp_obj_t stream_obj, mp_uint_t event_mask, int *errcode) {
    const mp_stream_p_t *stream_p = mp_get_stream(stream_obj);
    if (!stream_p || !stream_p->ioctl) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    mp_poll_ctl_ioctl_args_t args = { poll_obj, op, stream_obj, event_mask };
    mp_uint_t result = stream_p->ioctl(stream_obj, MP_STREAM_POLL_CTL, (uintptr_t)&args, errcode);
    if (result == MP_STREAM_ERROR && *errcode == MP_EINVAL) {
        *errcode = MP_EPERM;
    }
    return result;
}

// ###

STATIC void mp_poll_signal(mp_obj_t poll_obj, mp_obj_t stream_obj, mp_uint_t events, BaseType_t *pxHigherPriorityTaskWoken) {
    mp_obj_poll_t *self = MP_OBJ_FROM_PTR(poll_obj);
    assert(self->stream_obj == stream_obj);
    if (pxHigherPriorityTaskWoken) {
        xTaskNotifyFromISR(self->task, events, eSetBits, pxHigherPriorityTaskWoken);
    } else {
        xTaskNotify(self->task, events, eSetBits);
    }
}

STATIC const mp_poll_p_t mp_poll_p = {
    .signal = mp_poll_signal,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_poll,
    MP_QSTR_Poll,
    MP_TYPE_FLAG_NONE,
    protocol, &mp_poll_p
    );

STATIC void mp_poll_nlr_callback(void *ctx) {
    mp_obj_poll_t *self = ctx - offsetof(mp_obj_poll_t, nlr_callback);
    if (self->stream_obj) {
        int errcode;
        mp_poll_ctl(MP_OBJ_FROM_PTR(self), MP_POLL_CTL_DEL, self->stream_obj, 0, &errcode);
        self->stream_obj = MP_OBJ_NULL;
    }
}

void mp_poll_init(mp_obj_poll_t *self, const mp_obj_type_t *type, mp_obj_t stream_obj, mp_uint_t event_mask) {
    self->base.type = type ? type : &mp_type_poll;
    self->task = xTaskGetCurrentTaskHandle();
    self->stream_obj = stream_obj;
    int errcode;
    mp_uint_t events = mp_poll_ctl(MP_OBJ_FROM_PTR(self), MP_POLL_CTL_ADD, stream_obj, event_mask, &errcode);
    if (events == MP_STREAM_ERROR) {
        mp_raise_OSError(errcode);
    }

    nlr_push_jump_callback(&self->nlr_callback, mp_poll_nlr_callback);

    xTaskNotifyStateClear(NULL);
    ulTaskNotifyValueClear(NULL, -1);
}

void mp_poll_deinit(mp_obj_poll_t *self) {
    nlr_pop_jump_callback(true);
}

bool mp_poll_wait(mp_obj_poll_t *self, TickType_t *timeout) {
    uint32_t e = 0;
    BaseType_t ok = pdFALSE;
    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    while (!ok && !xTaskCheckForTimeOut(&xTimeOut, timeout)) {
        while (thread_enable_interrupt()) {
            mp_handle_pending(true);
        }

        MP_THREAD_GIL_EXIT();
        ok = xTaskNotifyWait(0, -1, &e, *timeout);
        thread_disable_interrupt();
        MP_THREAD_GIL_ENTER();
    }
    return ok;
}

mp_uint_t mp_poll_block(mp_obj_t stream_obj, void *buf, mp_uint_t size, int *errcode, mp_uint_t (*func)(mp_obj_t, void *, mp_uint_t, int *), mp_uint_t events, TickType_t xTicksToWait, bool greedy) {
    mp_uint_t ret = func(stream_obj, buf, size, errcode);
    if (xTicksToWait == 0) {
        // out of time, return whatever we got
        return ret;
    }
    if (ret == MP_STREAM_ERROR) {
        if (!mp_is_nonblocking_error(*errcode)) {
            // non-non-blocking error, return it
            return ret;
        }
    } else if ((ret >= size) || !greedy) {
        // we processed all the data, or we are not greedy
        return ret;
    } else {
        buf += ret;
        size -= ret;
    }

    // start up machinery for doing blocking wait
    mp_obj_poll_t poll;
    mp_poll_init(&poll, NULL, stream_obj, events);

    mp_uint_t total_ret = ret;
    do {
        ret = func(stream_obj, buf, size, errcode);
        if (ret == MP_STREAM_ERROR) {
            if (!mp_is_nonblocking_error(*errcode)) {
                // non-non-blocking error, return it
                total_ret = ret;
                break;
            }
        } else {
            // valid result, update our total
            total_ret = (total_ret == MP_STREAM_ERROR ? 0 : total_ret) + ret;
            if ((ret >= size) || !greedy) {
                // we processed all the data, or we are not greedy
                break;
            }
            buf += ret;
            size -= ret;
        }
    }
    while (mp_poll_wait(&poll, &xTicksToWait));

    mp_poll_deinit(&poll);
    return total_ret;
}
