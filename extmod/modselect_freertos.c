// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "py/mpconfig.h"
#if (MICROPY_PY_SELECT && MICROPY_FREERTOS)
#include "FreeRTOS.h"
#include "queue.h"

#include "newlib/thread.h"

#include "py/runtime.h"
#include "py/obj.h"
#include "py/objlist.h"
#include "py/stream_poll.h"
#include "py/mperrno.h"
#include "py/poll.h"


static const qstr selector_key_attrs[] = { MP_QSTR_fileobj, MP_QSTR_events, MP_QSTR_data };

typedef struct {
    mp_uint_t events;
    mp_obj_t stream_obj;
} select_event_t;

typedef struct {
    mp_obj_base_t base;
    mp_map_t map;
    QueueHandle_t queue;
    StaticQueue_t queue_buffer;
    select_event_t queue_storage[];
} select_obj_selector_t;

static mp_obj_t select_selector_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);
    size_t queue_length = 16;
    if (n_args >= 1) {
        queue_length = mp_obj_get_int(args[0]);
    }

    select_obj_selector_t *self = m_new_obj_var(select_obj_selector_t, queue_storage, select_event_t, queue_length);
    self->base.type = type;
    mp_map_init(&self->map, queue_length);
    self->queue = xQueueCreateStatic(queue_length, sizeof(select_event_t), (uint8_t *)self->queue_storage, &self->queue_buffer);
    return MP_OBJ_FROM_PTR(self);
}

static void select_selector_signal(mp_obj_t poll_obj, mp_obj_t stream_obj, mp_uint_t events, BaseType_t *pxHigherPriorityTaskWoken) {
    select_obj_selector_t *self = MP_OBJ_TO_PTR(poll_obj);
    select_event_t event = { events, stream_obj };
    if (pxHigherPriorityTaskWoken) {
        if (!xQueueSendFromISR(self->queue, &event, pxHigherPriorityTaskWoken)) {
            assert(0);
        }
    } else {
        if (!xQueueSend(self->queue, &event, 0)) {
            assert(0);
        }
    }
}

const mp_poll_p_t select_selector_poll_p = {
    .signal = select_selector_signal,
};

static mp_obj_t select_selector_register(size_t n_args, const mp_obj_t *args) {
    // mp_arg_check_num(n_args, 0, 2, 4, false);
    select_obj_selector_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t stream_obj = args[1];
    mp_uint_t event_mask = MP_STREAM_POLL_RD | MP_STREAM_POLL_WR;
    if (n_args >= 3) {
        event_mask = mp_obj_get_int(args[2]);
    }
    mp_obj_t data = mp_const_none;
    if (n_args >= 4) {
        data = args[3];
    }

    mp_map_elem_t *elem = mp_map_lookup(&self->map, mp_obj_id(stream_obj), MP_MAP_LOOKUP);
    if (elem != MP_OBJ_NULL) {
        mp_raise_type(&mp_type_KeyError);
    }

    int errcode;
    mp_uint_t events = mp_poll_ctl(args[0], MP_POLL_CTL_ADD, stream_obj, event_mask, &errcode);
    if (events == MP_STREAM_ERROR) {
        mp_raise_OSError(errcode);
    } /*else if (events != 0) {
        select_selector_signal(args[0], stream_obj, events, NULL);
    }*/

    mp_obj_t items[] = { stream_obj, MP_OBJ_NEW_SMALL_INT(event_mask), data };
    mp_obj_t selector_key = mp_obj_new_attrtuple(selector_key_attrs, 3, items);

    elem = mp_map_lookup(&self->map, mp_obj_id(stream_obj), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
    elem->value = selector_key;
    return selector_key;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(select_selector_register_obj, 2, 4, select_selector_register);

static mp_obj_t select_selector_unregister(mp_obj_t self_in, mp_obj_t stream_obj) {
    select_obj_selector_t *self = MP_OBJ_TO_PTR(self_in);

    mp_map_elem_t *elem = mp_map_lookup(&self->map, mp_obj_id(stream_obj), MP_MAP_LOOKUP_REMOVE_IF_FOUND);
    if (elem == MP_OBJ_NULL) {
        mp_raise_type(&mp_type_KeyError);
    }

    int errcode;
    mp_uint_t result = mp_poll_ctl(self_in, MP_POLL_CTL_DEL, stream_obj, 0, &errcode);
    if (result == MP_STREAM_ERROR) {
        mp_raise_OSError(errcode);
    }

    return elem->value;
}
MP_DEFINE_CONST_FUN_OBJ_2(select_selector_unregister_obj, select_selector_unregister);


static mp_obj_t select_selector_modify(size_t n_args, const mp_obj_t *args) {
    // mp_arg_check_num(n_args, 0, 2, 4, false);
    select_obj_selector_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_obj_t stream_obj = args[1];
    mp_uint_t event_mask = MP_STREAM_POLL_RD | MP_STREAM_POLL_WR;
    if (n_args >= 3) {
        event_mask = mp_obj_get_int(args[2]);
    }
    mp_obj_t data = mp_const_none;
    if (n_args >= 4) {
        data = args[3];
    }

    mp_map_elem_t *elem = mp_map_lookup(&self->map, mp_obj_id(stream_obj), MP_MAP_LOOKUP);
    if (elem == MP_OBJ_NULL) {
        mp_raise_type(&mp_type_KeyError);
    }

    int errcode;
    mp_uint_t events = mp_poll_ctl(args[0], MP_POLL_CTL_MOD, stream_obj, event_mask, &errcode);
    if (events == MP_STREAM_ERROR) {
        mp_raise_OSError(errcode);
    }

    mp_obj_t items[] = { stream_obj, MP_OBJ_NEW_SMALL_INT(event_mask), data };
    mp_obj_t selector_key = mp_obj_new_attrtuple(selector_key_attrs, 3, items);
    elem->value = selector_key;
    return selector_key;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(select_selector_modify_obj, 2, 4, select_selector_modify);


static mp_obj_t select_selector_select(size_t n_args, const mp_obj_t *args) {
    // mp_arg_check_num(n_args, 0, 1, 2, false);
    select_obj_selector_t *self = MP_OBJ_TO_PTR(args[0]);
    TickType_t timeout = portMAX_DELAY;
    if (n_args >= 2 && args[1] != mp_const_none) {
        mp_float_t timeout_s = mp_obj_get_float(args[1]);
        mp_int_t timeout_ms = 1000.0 * timeout_s + 0.5;
        timeout = pdMS_TO_TICKS(MAX(0, timeout_ms));
    }

    select_event_t event;
    BaseType_t ok = xQueueReceive(self->queue, &event, 0);

    TimeOut_t xTimeOut;
    vTaskSetTimeOutState(&xTimeOut);
    while (!ok && !xTaskCheckForTimeOut(&xTimeOut, &timeout)) {
        while (thread_enable_interrupt()) {
            mp_handle_pending(true);
        }

        MP_THREAD_GIL_EXIT();
        ok = xQueueReceive(self->queue, &event, timeout);
        thread_disable_interrupt();
        MP_THREAD_GIL_ENTER();
    }

    mp_obj_t result = mp_obj_new_list(0, NULL);
    while (ok) {
        mp_map_lookup_kind_t lookup_kind = event.events & MP_STREAM_POLL_NVAL ? MP_MAP_LOOKUP_REMOVE_IF_FOUND : MP_MAP_LOOKUP;
        mp_map_elem_t *elem = mp_map_lookup(&self->map, mp_obj_id(event.stream_obj), lookup_kind);
        if (elem != MP_OBJ_NULL) {
            mp_obj_t items[] = { elem->value, MP_OBJ_NEW_SMALL_INT(event.events) };
            mp_obj_t tuple = mp_obj_new_tuple(2, items);
            mp_obj_list_append(result, tuple);
        }
        ok = xQueueReceive(self->queue, &event, 0);
    }
    return result;
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(select_selector_select_obj, 1, 2, select_selector_select);

static mp_obj_t select_selector_close(mp_obj_t self_in) {
    select_obj_selector_t *self = MP_OBJ_TO_PTR(self_in);

    for (size_t i = 0; i < self->map.alloc; i++) {
        if (!mp_map_slot_is_filled(&self->map, i)) {
            continue;
        }

        mp_map_elem_t *elem = &self->map.table[i];
        size_t tuple_len;
        mp_obj_t *tuple_items;
        mp_obj_tuple_get(elem->value, &tuple_len, &tuple_items);
        assert(tuple_len == 3);
        mp_obj_t stream_obj = tuple_items[0];
        int errcode;
        mp_uint_t result = mp_poll_ctl(self_in, MP_POLL_CTL_DEL, stream_obj, 0, &errcode);
        if (result == MP_STREAM_ERROR) {
            mp_raise_OSError(errcode);
        }
    }

    vQueueDelete(self->queue);

    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(select_selector_close_obj, select_selector_close);

static mp_obj_t select_selector_get_key(mp_obj_t self_in, mp_obj_t stream_obj) {
    select_obj_selector_t *self = MP_OBJ_TO_PTR(self_in);
    mp_map_elem_t *elem = mp_map_lookup(&self->map, mp_obj_id(stream_obj), MP_MAP_LOOKUP);
    if (elem == MP_OBJ_NULL) {
        mp_raise_type(&mp_type_KeyError);
    }
    return elem->value;
}
MP_DEFINE_CONST_FUN_OBJ_2(select_selector_get_key_obj, select_selector_get_key);

static mp_obj_t select_selector_get_map(mp_obj_t self_in) {
    select_obj_selector_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_t result = mp_obj_new_dict(self->map.used);
    for (int i = 0; i < self->map.alloc; i++) {
        mp_map_elem_t *elem = &self->map.table[i];
        if (elem->key) {
            mp_obj_dict_store(result, elem->key, elem->value);
        }
    }
    return result;
}
MP_DEFINE_CONST_FUN_OBJ_1(select_selector_get_map_obj, select_selector_get_map);

static const mp_rom_map_elem_t select_selector_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_register),        MP_ROM_PTR(&select_selector_register_obj) },
    { MP_ROM_QSTR(MP_QSTR_unregister),      MP_ROM_PTR(&select_selector_unregister_obj) },
    { MP_ROM_QSTR(MP_QSTR_modify),          MP_ROM_PTR(&select_selector_modify_obj) },
    { MP_ROM_QSTR(MP_QSTR_select),          MP_ROM_PTR(&select_selector_select_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&select_selector_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_key),         MP_ROM_PTR(&select_selector_get_key_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_map),         MP_ROM_PTR(&select_selector_get_map_obj) },
};
static MP_DEFINE_CONST_DICT(select_selector_locals_dict, select_selector_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    select_type_selector,
    MP_QSTR_Selector,
    MP_TYPE_FLAG_NONE,
    make_new, &select_selector_make_new,
    protocol, &select_selector_poll_p,
    locals_dict, &select_selector_locals_dict
    );


// ### event ###
typedef struct {
    mp_obj_base_t base;
    uint64_t value;
    mp_stream_poll_t poll;
} select_obj_event_t;

static mp_obj_t select_event_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, false);

    select_obj_event_t *self = m_new_obj(select_obj_event_t);
    self->base.type = type;
    self->value = 0;
    mp_stream_poll_init(&self->poll);

    if (n_args >= 1) {
        self->value = mp_obj_get_int(args[0]);
    }
    return MP_OBJ_FROM_PTR(self);
}

static mp_uint_t event_close(mp_obj_t self_in, int *errcode) {
    select_obj_event_t *self = MP_OBJ_TO_PTR(self_in);
    mp_stream_poll_close(&self->poll);
    return 0;
}

static mp_uint_t select_event_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    select_obj_event_t *self = MP_OBJ_TO_PTR(self_in);
    if (size < sizeof(uint64_t)) {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }

    if (self->value == 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }

    if (self->value == -1) {
        mp_stream_poll_signal(&self->poll, MP_STREAM_POLL_WR, NULL);
    }
    *(uint64_t *)buf = self->value;
    self->value = 0;
    return sizeof(uint64_t);
}

static mp_uint_t select_event_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    select_obj_event_t *self = MP_OBJ_TO_PTR(self_in);
    if (size < sizeof(uint64_t)) {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }

    uint64_t x = *(uint64_t *)buf;
    if (self->value + x < x) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }

    if (self->value == 0 && x > 0) {
        mp_stream_poll_signal(&self->poll, MP_STREAM_POLL_RD, NULL);
    }
    self->value += x;
    return sizeof(uint64_t);
}

static mp_uint_t select_event_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    select_obj_event_t *self = MP_OBJ_TO_PTR(self_in);

    mp_uint_t events;
    switch (request) {
        case MP_STREAM_POLL:
            events = (self->value ? MP_STREAM_POLL_RD : 0) | (~self->value ? MP_STREAM_POLL_WR : 0);
            return events & arg;
        case MP_STREAM_CLOSE:
            return event_close(self, errcode);
        case MP_STREAM_POLL_CTL:
            return mp_stream_poll_ctl(&self->poll, (void *)arg, errcode);
        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

static const mp_rom_map_elem_t select_event_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_read),            MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),           MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_ioctl),           MP_ROM_PTR(&mp_stream_ioctl_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_stream_close_obj) },
};
static MP_DEFINE_CONST_DICT(select_event_locals_dict, select_event_locals_dict_table);

static const mp_stream_p_t select_event_stream_p = {
    .read = select_event_read,
    .write = select_event_write,
    .ioctl = select_event_ioctl,
};

static MP_DEFINE_CONST_OBJ_TYPE(
    select_type_event,
    MP_QSTR_Event,
    MP_TYPE_FLAG_NONE,
    make_new, &select_event_make_new,
    protocol, &select_event_stream_p,
    locals_dict, &select_event_locals_dict
    );


static const mp_rom_map_elem_t select_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_select) },
    { MP_ROM_QSTR(MP_QSTR_Selector),        MP_ROM_PTR(&select_type_selector) },
    { MP_ROM_QSTR(MP_QSTR_Event),           MP_ROM_PTR(&select_type_event) },
    { MP_ROM_QSTR(MP_QSTR_POLLIN),          MP_ROM_INT(MP_STREAM_POLL_RD) },
    { MP_ROM_QSTR(MP_QSTR_POLLOUT),         MP_ROM_INT(MP_STREAM_POLL_WR) },
    { MP_ROM_QSTR(MP_QSTR_POLLERR),         MP_ROM_INT(MP_STREAM_POLL_ERR) },
    { MP_ROM_QSTR(MP_QSTR_POLLHUP),         MP_ROM_INT(MP_STREAM_POLL_HUP) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_READ),      MP_ROM_INT(MP_STREAM_POLL_RD) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_WRITE),     MP_ROM_INT(MP_STREAM_POLL_WR) },
};

static MP_DEFINE_CONST_DICT(select_module_globals, select_module_globals_table);

const mp_obj_module_t select_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&select_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_select, select_module);

#endif // MICROPY_PY_USELECT
