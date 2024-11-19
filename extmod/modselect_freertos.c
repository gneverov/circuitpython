// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include "morelib/poll.h"

#include "FreeRTOS.h"

#include "extmod/modos_newlib.h"
#include "py/runtime.h"
#include "py/obj.h"


typedef struct {
    mp_obj_base_t base;
    struct pollfd *fds;
    nfds_t nfds;
    size_t size;
} select_obj_poll_t;

static mp_obj_t select_poll_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    select_obj_poll_t *self = m_new_obj(select_obj_poll_t);
    self->base.type = type;
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t select_poll_register(size_t n_args, const mp_obj_t *args) {
    select_obj_poll_t *self = MP_OBJ_TO_PTR(args[0]);
    int fd = mp_os_get_fd(args[1]);
    uint events = (n_args > 2) ? mp_obj_get_int(args[2]) : POLLIN | POLLPRI | POLLOUT;

    int i = 0;
    for (; i < self->nfds; i++) {
        if (self->fds[i].fd == fd) {
            goto found;
        }
    }
    i = self->nfds++;
    self->fds = m_realloc(self->fds, self->nfds * sizeof(struct pollfd));
    self->fds[i].fd = fd;

found:
    self->fds[i].events = events;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(select_poll_register_obj, 2, 3, select_poll_register);

static mp_obj_t select_poll_unregister(mp_obj_t self_in, mp_obj_t fd_in) {
    select_obj_poll_t *self = MP_OBJ_TO_PTR(self_in);
    int fd = mp_os_get_fd(fd_in);

    int i = 0;
    for (; i < self->nfds; i++) {
        if (self->fds[i].fd == fd) {
            goto found;
        }
    }
    mp_raise_type(&mp_type_KeyError);

found:
    self->nfds--;
    for (; i < self->nfds; i++) {
        self->fds[i] = self->fds[i + 1];
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(select_poll_unregister_obj, select_poll_unregister);

static mp_obj_t select_poll_modify(mp_obj_t self_in, mp_obj_t fd_in, mp_obj_t events_in) {
    select_obj_poll_t *self = MP_OBJ_TO_PTR(self_in);
    int fd = mp_os_get_fd(fd_in);
    uint events = mp_obj_get_int(events_in);

    for (int i = 0; i < self->nfds; i++) {
        if (self->fds[i].fd == fd) {
            self->fds[i].events = events;
            return mp_const_none;
        }
    }
    mp_raise_OSError(ENOENT);
}
static MP_DEFINE_CONST_FUN_OBJ_3(select_poll_modify_obj, select_poll_modify);

static mp_obj_t select_poll_poll(size_t n_args, const mp_obj_t *args) {
    select_obj_poll_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_int_t timeout_ms = (n_args > 1 && args[1] != mp_const_none) ? mp_obj_get_int(args[1]) : -1;
    TickType_t timeout = timeout_ms >= 0 ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;

    int ret;
    MP_OS_CALL(ret, poll_ticks, self->fds, self->nfds, &timeout);
    mp_os_check_ret(ret);

    mp_obj_t result = mp_obj_new_list(ret, NULL);
    for (int i = 0; i < self->nfds; i++) {
        struct pollfd *pollfd = &self->fds[i];
        if (pollfd->revents) {
            mp_obj_t items[] = { MP_OBJ_NEW_SMALL_INT(pollfd->fd), MP_OBJ_NEW_SMALL_INT(pollfd->revents) };
            mp_obj_t tuple = mp_obj_new_tuple(2, items);
            mp_obj_list_append(result, tuple);
        }
    }
    return result;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(select_poll_poll_obj, 1, 2, select_poll_poll);

static const mp_rom_map_elem_t select_poll_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_register),    MP_ROM_PTR(&select_poll_register_obj) },
    { MP_ROM_QSTR(MP_QSTR_unregister),  MP_ROM_PTR(&select_poll_unregister_obj) },
    { MP_ROM_QSTR(MP_QSTR_modify),      MP_ROM_PTR(&select_poll_modify_obj) },
    { MP_ROM_QSTR(MP_QSTR_poll),        MP_ROM_PTR(&select_poll_poll_obj) },
};
static MP_DEFINE_CONST_DICT(select_poll_locals_dict, select_poll_locals_dict_table);

static MP_DEFINE_CONST_OBJ_TYPE(
    select_type_poll,
    MP_QSTR_poll,
    MP_TYPE_FLAG_NONE,
    make_new, &select_poll_make_new,
    locals_dict, &select_poll_locals_dict
    );

static const mp_rom_map_elem_t select_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_select) },
    { MP_ROM_QSTR(MP_QSTR_poll),        MP_ROM_PTR(&select_type_poll) },
    { MP_ROM_QSTR(MP_QSTR_POLLIN),      MP_ROM_INT(POLLIN) },
    { MP_ROM_QSTR(MP_QSTR_POLLPRI),     MP_ROM_INT(POLLPRI) },
    { MP_ROM_QSTR(MP_QSTR_POLLOUT),     MP_ROM_INT(POLLOUT) },
    { MP_ROM_QSTR(MP_QSTR_POLLERR),     MP_ROM_INT(POLLERR) },
    { MP_ROM_QSTR(MP_QSTR_POLLHUP),     MP_ROM_INT(POLLHUP) },
    { MP_ROM_QSTR(MP_QSTR_POLLNVAL),    MP_ROM_INT(POLLNVAL) },
};

static MP_DEFINE_CONST_DICT(select_module_globals, select_module_globals_table);

const mp_obj_module_t select_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&select_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_select, select_module);
