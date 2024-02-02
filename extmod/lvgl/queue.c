// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <memory.h>

#include "./modlvgl.h"
#include "./queue.h"


lvgl_queue_t *lvgl_queue_default;

lvgl_queue_t *lvgl_queue_alloc(size_t size) {
    size_t mem_size = sizeof(lvgl_queue_t) + size * sizeof(lvgl_queue_elem_t *);
    lvgl_queue_t *queue = malloc(mem_size);
    assert(queue);
    memset(queue, 0, mem_size);
    queue->ref_count = 1;
    queue->obj = MP_OBJ_NULL;
    mp_stream_poll_init(&queue->poll);
    queue->size = size;
    return queue;
}

lvgl_queue_t *lvgl_queue_copy(lvgl_queue_t *queue) {
    assert(lvgl_is_locked());
    queue->ref_count++;
    return queue;
}

static void lvgl_queue_clear(lvgl_queue_t *queue) {
    while (queue->read_index < queue->write_index) {
        lvgl_queue_elem_t *elem = queue->ring[queue->read_index++ % queue->size];
        elem->del(elem);
    }
}

void lvgl_queue_free(lvgl_queue_t *queue) {
    assert(lvgl_is_locked());
    assert(queue->ref_count > 0);
    if (--queue->ref_count == 0) {
        lvgl_queue_clear(queue);
        free(queue);
    }
}

mp_obj_t lvgl_queue_from(lvgl_queue_t *queue) {
    if (!queue) {
        return mp_const_none;
    }
    lvgl_obj_queue_t *self = queue->obj;
    if (!self) {
        self = m_new_obj_with_finaliser(lvgl_obj_queue_t);
        lvgl_lock();
        self->base.type = &lvgl_type_queue;
        self->queue = lvgl_queue_copy(queue);
        self->timeout = portMAX_DELAY;
        queue->obj = self;
        lvgl_unlock();
    }
    return MP_OBJ_FROM_PTR(self);
}

void lvgl_queue_send(lvgl_queue_t *queue, lvgl_queue_elem_t *elem) {
    assert(lvgl_is_locked());
    assert(queue->obj);

    if ((queue->write_index - queue->read_index) >= queue->size) {
        queue->writer_overflow = 1;
        elem->del(elem);
        return;
    }

    queue->ring[queue->write_index++ % queue->size] = elem;
    mp_stream_poll_signal(&queue->poll, MP_STREAM_POLL_RD, NULL);
}

lvgl_queue_elem_t *lvgl_queue_receive(lvgl_queue_t *queue) {
    assert(lvgl_is_locked());
    if (queue->read_index == queue->write_index) {
        return NULL;
    }

    lvgl_queue_elem_t *elem = queue->ring[queue->read_index++ % queue->size];
    queue->writer_overflow = 0;
    return elem;
}

// STATIC mp_obj_t lvgl_obj_queue_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
//     mp_arg_check_num(n_args, n_kw, 0, 0, false);

//     lvgl_obj_queue_t *self = m_new_obj_with_finaliser(lvgl_obj_queue_t);
//     self->base.type = type;
//     self->queue = lvgl_queue_alloc(self);
//     return MP_OBJ_FROM_PTR(self);
// }

STATIC void lvgl_obj_queue_deinit(lvgl_obj_queue_t *self ) {
    lvgl_queue_t *queue = self->queue;
    if (queue) {
        lvgl_lock();
        assert(queue->obj = self);
        queue->obj = NULL;
        lvgl_queue_free(queue);
        lvgl_unlock();
        self->queue = NULL;
    }
}

STATIC mp_obj_t lvgl_obj_queue_del(mp_obj_t self_in) {
    lvgl_obj_queue_t *self = MP_OBJ_TO_PTR(self_in);
    lvgl_obj_queue_deinit(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_obj_queue_del_obj, lvgl_obj_queue_del);

STATIC mp_uint_t lvgl_obj_queue_close(mp_obj_t self_in, int *errcode) {
    lvgl_obj_queue_t *self = MP_OBJ_TO_PTR(self_in);
    lvgl_obj_queue_deinit(self);
    return 0;
}

STATIC mp_uint_t lvgl_obj_queue_run_nonblock(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    lvgl_obj_queue_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->queue) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    lvgl_lock();
    lvgl_queue_elem_t *elem = lvgl_queue_receive(self->queue);
    bool closed = self->queue->writer_closed;
    lvgl_unlock();

    if (!elem) {
        if (closed) {
            return 0;
        }
        else {
            *errcode = MP_EAGAIN;
            return MP_STREAM_ERROR;
        }
    }

    nlr_buf_t nlr;
    volatile unsigned int r = nlr_push(&nlr);
    if (r == 0) {
        elem->run(elem);
        nlr_pop();
    }

    lvgl_lock();
    elem->del(elem);
    lvgl_unlock();

    if (r != 0) {
        nlr_jump(nlr.ret_val);
    }
    return 1;
}

STATIC mp_obj_t lvgl_obj_queue_run(mp_obj_t self_in) {
    lvgl_obj_queue_t *self = MP_OBJ_TO_PTR(self_in);
    int errcode;
    mp_uint_t ret = mp_poll_block(self_in, NULL, 1, &errcode, lvgl_obj_queue_run_nonblock, MP_STREAM_POLL_RD, self->timeout, false);
    return mp_stream_return(ret, errcode);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_obj_queue_run_obj, lvgl_obj_queue_run);

STATIC mp_uint_t lvgl_obj_queue_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    lvgl_obj_queue_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->queue && (request != MP_STREAM_CLOSE)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    mp_uint_t ret;
    switch (request) {
        case MP_STREAM_TIMEOUT:
            ret = mp_stream_timeout(&self->timeout, arg, errcode);
            break;
        case MP_STREAM_POLL_CTL:
            lvgl_lock();
            ret = mp_stream_poll_ctl(&self->queue->poll, (void *)arg, errcode);
            lvgl_unlock();
            break;
        case MP_STREAM_CLOSE:
            ret = lvgl_obj_queue_close(self_in, errcode);
            break;
        default:
            *errcode = MP_EINVAL;
            ret = MP_STREAM_ERROR;
            break;
    }
    return ret;
}

STATIC const mp_stream_p_t lvgl_obj_queue_p = {
    .ioctl = lvgl_obj_queue_ioctl,
};

STATIC const mp_rom_map_elem_t lvgl_obj_queue_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_obj_queue_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_run),             MP_ROM_PTR(&lvgl_obj_queue_run_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout),      MP_ROM_PTR(&mp_stream_settimeout_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_obj_queue_locals_dict, lvgl_obj_queue_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_queue,
    MP_QSTR_Queue,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    // make_new, lvgl_obj_queue_make_new,
    protocol, &lvgl_obj_queue_p,
    locals_dict, &lvgl_obj_queue_locals_dict 
    );
