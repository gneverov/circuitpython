// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <termios.h>

#include "extmod/modos_newlib.h"
#include "py/runtime.h"


static mp_obj_t mp_termios_tcgetattr(mp_obj_t fd_in) {
    int fd = mp_os_get_fd(fd_in);
    struct termios p;
    int ret = tcgetattr(fd, &p);
    mp_os_check_ret(ret);

    mp_obj_t ccs[NCCS];
    for (int i = 0; i < NCCS; i++) {
        if ((i == VMIN) || (i == VTIME)) {
            ccs[i] = MP_OBJ_NEW_SMALL_INT(p.c_cc[i]);
        } else {
            ccs[i] = mp_obj_new_bytes(&p.c_cc[i], 1);
        }
    }
    mp_obj_t items[7] = {
        mp_obj_new_int(p.c_iflag),
        mp_obj_new_int(p.c_oflag),
        mp_obj_new_int(p.c_cflag),
        mp_obj_new_int(p.c_lflag),
        mp_obj_new_int(cfgetispeed(&p)),
        mp_obj_new_int(cfgetospeed(&p)),
        mp_obj_new_list(NCCS, ccs),
    };
    return mp_obj_new_list(7, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_termios_tcgetattr_obj, mp_termios_tcgetattr);

static mp_obj_t mp_termios_tcsetattr(mp_obj_t fd_in, mp_obj_t when_in, mp_obj_t attributes_in) {
    int fd = mp_os_get_fd(fd_in);
    int when = mp_obj_get_int(when_in);
    size_t attr_len;
    mp_obj_t *attrs;
    mp_obj_list_get(attributes_in, &attr_len, &attrs);
    if (attr_len != 7) {
        mp_raise_ValueError(NULL);
    }

    struct termios p;
    p.c_iflag = mp_obj_get_int(attrs[0]);
    p.c_oflag = mp_obj_get_int(attrs[1]);
    p.c_cflag = mp_obj_get_int(attrs[2]);
    p.c_lflag = mp_obj_get_int(attrs[3]);
    cfsetispeed(&p, mp_obj_get_int(attrs[4]));
    cfsetospeed(&p, mp_obj_get_int(attrs[5]));

    size_t cc_len;
    mp_obj_t *ccs;
    mp_obj_list_get(attrs[6], &cc_len, &ccs);
    for (int i = 0; i < NCCS; i++) {
        if ((i == VMIN) || (i == VTIME)) {
            p.c_cc[i] = mp_obj_get_int(ccs[i]);
            continue;
        }
        size_t len;
        const char *c = mp_obj_str_get_data(ccs[i], &len);
        if (len < 1) {
            mp_raise_ValueError(NULL);
        }
        p.c_cc[i] = *c;
    }

    int ret = tcsetattr(fd, when, &p);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(mp_termios_tcsetattr_obj, mp_termios_tcsetattr);

static mp_obj_t mp_termios_tcsendbreak(mp_obj_t fd_in, mp_obj_t duration_in) {
    int fd = mp_os_get_fd(fd_in);
    int duration = mp_obj_get_int(duration_in);
    int ret = tcsendbreak(fd, duration);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_termios_tcsendbreak_obj, mp_termios_tcsendbreak);

static mp_obj_t mp_termios_tcdrain(mp_obj_t fd_in) {
    int fd = mp_os_get_fd(fd_in);
    int ret = tcdrain(fd);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_termios_tcdrain_obj, mp_termios_tcdrain);

static mp_obj_t mp_termios_tcflush(mp_obj_t fd_in, mp_obj_t queue_in) {
    int fd = mp_os_get_fd(fd_in);
    int queue = mp_obj_get_int(queue_in);
    int ret = tcflush(fd, queue);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_termios_tcflush_obj, mp_termios_tcflush);

static mp_obj_t mp_termios_tcflow(mp_obj_t fd_in, mp_obj_t action_in) {
    int fd = mp_os_get_fd(fd_in);
    int action = mp_obj_get_int(action_in);
    int ret = tcflow(fd, action);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_termios_tcflow_obj, mp_termios_tcflow);

static mp_obj_t mp_termios_setraw(size_t n_args, const mp_obj_t *args) {
    int fd = mp_os_get_fd(args[0]);
    int when = (n_args > 1) ? mp_obj_get_int(args[1]) : TCSAFLUSH;

    mp_obj_t out = mp_termios_tcgetattr(args[0]);

    struct termios p;
    int ret = tcgetattr(fd, &p);
    mp_os_check_ret(ret);

    p.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    p.c_oflag &= ~OPOST;
    p.c_cflag &= ~(CSIZE | PARENB);
    p.c_cflag |= CS8;
    p.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

    ret = tcsetattr(fd, when, &p);
    mp_os_check_ret(ret);
    return out;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_termios_setraw_obj, 1, 2, mp_termios_setraw);

// static mp_obj_t mp_termios_setcbreak(size_t n_args, const mp_obj_t *args) {
//     int fd = mp_os_get_fd(args[0]);
//     int when = (n_args > 1) ? mp_obj_get_int(args[1]) : TCSAFLUSH;

//     mp_obj_t out = mp_termios_tcgetattr(args[0]);

//     struct termios p;
//     int ret = tcgetattr(fd, &p);
//     mp_os_check_ret(ret);

//     p.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
//     p.c_oflag &= ~OPOST;
//     p.c_cflag &= ~(CSIZE | PARENB);
//     p.c_cflag |= CS8;
//     p.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
//     p.c_lflag |= ISIG;

//     ret = tcsetattr(fd, when, &p);
//     mp_os_check_ret(ret);
//     return out;
// }
// static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_termios_setcbreak_obj, 1, 2, mp_termios_setcbreak);

static const mp_rom_map_elem_t mp_termios_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_termios) },
    { MP_ROM_QSTR(MP_QSTR_tcgetattr),   MP_ROM_PTR(&mp_termios_tcgetattr_obj) },
    { MP_ROM_QSTR(MP_QSTR_tcsetattr),   MP_ROM_PTR(&mp_termios_tcsetattr_obj) },
    { MP_ROM_QSTR(MP_QSTR_tcsendbreak), MP_ROM_PTR(&mp_termios_tcsendbreak_obj) },
    { MP_ROM_QSTR(MP_QSTR_tcdrain),     MP_ROM_PTR(&mp_termios_tcdrain_obj) },
    { MP_ROM_QSTR(MP_QSTR_tcflush),     MP_ROM_PTR(&mp_termios_tcflush_obj) },
    { MP_ROM_QSTR(MP_QSTR_tcflow),      MP_ROM_PTR(&mp_termios_tcflow_obj) },
    { MP_ROM_QSTR(MP_QSTR_setraw),      MP_ROM_PTR(&mp_termios_setraw_obj) },
    // { MP_ROM_QSTR(MP_QSTR_setcbreak),   MP_ROM_PTR(&mp_termios_setcbreak_obj) },

    { MP_ROM_QSTR(MP_QSTR_TCSANOW),     MP_ROM_INT(TCSANOW) },
    { MP_ROM_QSTR(MP_QSTR_TCSADRAIN),   MP_ROM_INT(TCSADRAIN) },
    { MP_ROM_QSTR(MP_QSTR_TCSAFLUSH),   MP_ROM_INT(TCSAFLUSH) },
};
static MP_DEFINE_CONST_DICT(mp_termios_module_globals, mp_termios_module_globals_table);

const mp_obj_module_t mp_module_termios = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_termios_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_termios, mp_module_termios);
MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_tty, mp_module_termios);
