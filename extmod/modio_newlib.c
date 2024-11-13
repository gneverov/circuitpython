// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "extmod/modio_newlib.h"
#include "extmod/modos_newlib.h"
#include "py/builtin.h"
#include "py/objstr.h"
#include "py/parseargs.h"
#include "py/stream.h"
#include "py/runtime.h"


mp_import_stat_t mp_import_stat(const char *path) {
    struct stat buf;
    int ret = stat(path, &buf);
    if (ret >= 0) {
        if (S_ISDIR(buf.st_mode)) {
            return MP_IMPORT_STAT_DIR;
        }
        if (S_ISREG(buf.st_mode)) {
            return MP_IMPORT_STAT_FILE;
        }
    }
    return MP_IMPORT_STAT_NO_EXIST;
}

static mp_obj_t mp_io_file_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);
static mp_obj_t mp_io_text_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);

mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    mp_obj_t file;
    mp_obj_t mode = MP_OBJ_NEW_QSTR(MP_QSTR_r);
    mp_obj_t buffering = MP_OBJ_NEW_SMALL_INT(-1);
    mp_obj_t encoding = mp_const_none;
    mp_obj_t errors = mp_const_none;
    mp_obj_t newline = mp_const_none;
    mp_obj_t closefd = mp_const_true;
    mp_obj_t opener = mp_const_none;
    const qstr kws[] = { MP_QSTR_file, MP_QSTR_mode, MP_QSTR_buffering, MP_QSTR_encoding, MP_QSTR_errors, MP_QSTR_newline, MP_QSTR_closefd, MP_QSTR_opener, 0 };
    parse_args_and_kw_map(n_args, pos_args, kw_args, "O|OOOOOOO", kws, &file, &mode, &buffering, &encoding, &errors, &newline, &closefd, &opener);

    const char *mode_str = mp_obj_str_get_str(mode);
    bool text = true;
    while (*mode_str) {
        switch (*mode_str++) {
            case 'b':
                text = false;
                break;
            case 't':
                text = true;
                break;
        }
    }

    mp_obj_t file_args[4] = {
        file,
        mode,
        closefd,
        opener,
    };
    mp_obj_t ret_obj = mp_io_file_make_new(&mp_type_io_fileio, 4, 0, file_args);
    if (text) {
        mp_obj_t text_args[5] = {
            ret_obj,
            encoding,
            errors,
            newline,
            buffering,
        };
        ret_obj = mp_io_text_make_new(&mp_type_io_textio, 5, 0, text_args);
    }
    return ret_obj;
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);


// FileIO

static mp_obj_io_file_t *mp_io_file_get(mp_obj_t self_in) {
    // mp_obj_io_file_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_io_file_t *self = MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&mp_type_io_fileio)));
    if (self->fd == -1) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed file"));
    }
    return self;
}

static mp_obj_t mp_io_file_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_name, MP_QSTR_mode, MP_QSTR_closefd, MP_QSTR_opener, 0 };
    mp_obj_t name;
    const char *mode = "r";
    int closefd = 1;
    mp_obj_t opener = mp_const_none;
    parse_args_and_kw(n_args, n_kw, args, "O|spO", kws, &name, &mode, &closefd, &opener);

    int mode_rw = 0, mode_x = 0;
    while (*mode) {
        switch (*mode++) {
            case 'r':
                mode_rw = O_RDONLY;
                break;
            case 'w':
                mode_rw = O_WRONLY;
                mode_x = O_CREAT | O_TRUNC;
                break;
            case 'x':
                mode_x = O_CREAT | O_EXCL;
                break;
            case 'a':
                mode_rw = O_WRONLY;
                mode_x = O_CREAT | O_APPEND;
                break;
            case '+':
                mode_rw = O_RDWR;
                break;
        }
    }

    mp_obj_io_file_t *self = mp_obj_malloc_with_finaliser(mp_obj_io_file_t, type);
    mp_obj_t fd_obj = name;
    if (!mp_obj_is_int(name)) {
        if (!closefd) {
            mp_raise_ValueError(NULL);
        }
        mp_obj_t args[] = { name, MP_OBJ_NEW_SMALL_INT(mode_x | mode_rw) };
        fd_obj = mp_call_function_n_kw(opener == mp_const_none ? MP_OBJ_FROM_PTR(&mp_os_open_obj) : opener, 2, 0, args);
    }
    self->fd = mp_obj_get_int(fd_obj);
    self->name = name;
    self->mode = mp_obj_new_str(mode, strlen(mode));
    self->closefd = closefd;
    return MP_OBJ_FROM_PTR(self);
}

static void mp_io_file_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    mp_printf(print, "<io.%q>", (qstr)self->base.type->name);

    // mp_print_str(print, "<io.");
    // mp_obj_print_helper(print, MP_OBJ_NEW_QSTR(self->base.type->name), PRINT_STR);
    // mp_print_str(print, " name='");
    // mp_obj_print_helper(print, self->name, PRINT_STR);
    // mp_print_str(print, "' mode='");
    // mp_obj_print_helper(print, self->mode, PRINT_STR);
    // mp_print_str(print, "' closefd=");
    // mp_obj_print_helper(print, mp_obj_new_bool(self->closefd), PRINT_STR);
    // mp_print_str(print, ">");
}

static void mp_io_file_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_obj_io_file_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_SENTINEL) {
        return;
    }
    switch (attr) {
        case MP_QSTR_closed:
            dest[0] = mp_obj_new_bool(self->fd < 0);
            break;
        case MP_QSTR_mode:
            dest[0] = self->mode;
            break;
        case MP_QSTR_name:
            dest[0] = self->name;
            break;
        default:
            dest[1] = MP_OBJ_SENTINEL;
            break;
    }
}

static mp_obj_t mp_io_file_close(mp_obj_t self_in) {
    mp_obj_io_file_t *self = MP_OBJ_TO_PTR(self_in);
    if ((self->fd >= 0) && self->closefd) {
        close(self->fd);
    }
    self->fd = -1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_close_obj, mp_io_file_close);

static mp_obj_t mp_io_file_fileno(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    return MP_OBJ_NEW_SMALL_INT(self->fd);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_fileno_obj, mp_io_file_fileno);

static mp_obj_t mp_io_file_isatty(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    return mp_os_isatty(MP_OBJ_NEW_SMALL_INT(self->fd));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_isatty_obj, mp_io_file_isatty);

static mp_obj_t mp_io_file_readall(mp_obj_t self_in);

static mp_obj_t mp_io_file_read(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_file_t *self = mp_io_file_get(args[0]);
    mp_int_t opt_size = n_args > 1 ? mp_obj_get_int(args[1]) : -1;
    if (opt_size < 0) {
        return mp_io_file_readall(args[0]);
    }

    size_t size = opt_size;
    vstr_t out_buffer;
    vstr_init(&out_buffer, size);
    int ret = mp_os_read_vstr(self->fd, &out_buffer, size);
    if (mp_os_nonblocking_ret(ret)) {
        return mp_const_none;
    }
    mp_os_check_ret(ret);
    return mp_obj_new_bytes_from_vstr(&out_buffer);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_read_obj, 1, 2, mp_io_file_read);

static mp_obj_t mp_io_file_has_flags(mp_obj_t self_in, int flags) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    int ret = fcntl(self->fd, F_GETFL);
    mp_os_check_ret(ret);
    return mp_obj_new_bool(ret & flags);
}

static mp_obj_t mp_io_file_readable(mp_obj_t self_in) {
    return mp_io_file_has_flags(self_in, FREAD);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_readable_obj, mp_io_file_readable);

static mp_obj_t mp_io_file_readall(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);

    vstr_t out_buffer;
    vstr_init(&out_buffer, MP_OS_DEFAULT_BUFFER_SIZE);
    int ret = 1;
    while (ret > 0) {
        ret = mp_os_read_vstr(self->fd, &out_buffer, MP_OS_DEFAULT_BUFFER_SIZE);
        if (mp_os_nonblocking_ret(ret)) {
            ret = 0;
        }
        mp_os_check_ret(ret);
    }
    return mp_obj_new_bytes_from_vstr(&out_buffer);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_readall_obj, mp_io_file_readall);

static mp_obj_t mp_io_file_readinto(mp_obj_t self_in, mp_obj_t b_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(b_in, &bufinfo, MP_BUFFER_WRITE);

    vstr_t vstr;
    vstr_init_fixed_buf(&vstr, bufinfo.len, bufinfo.buf);
    int ret = mp_os_read_vstr(self->fd, &vstr, bufinfo.len);
    if (mp_os_nonblocking_ret(ret)) {
        return mp_const_none;
    }
    mp_os_check_ret(ret);
    return mp_obj_new_int(vstr_len(&vstr));
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_file_readinto_obj, mp_io_file_readinto);

static mp_obj_t mp_io_file_readline(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_file_t *self = mp_io_file_get(args[0]);
    size_t size = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;

    vstr_t out_buffer;
    vstr_init(&out_buffer, MIN(size, MP_OS_DEFAULT_BUFFER_SIZE));
    while (vstr_len(&out_buffer) < size) {
        int ret = mp_os_read_vstr(self->fd, &out_buffer, 1);
        if (mp_os_nonblocking_ret(ret)) {
            ret = 0;
        }
        mp_os_check_ret(ret);
        if ((ret == 0) || (vstr_str(&out_buffer)[vstr_len(&out_buffer) - 1] == '\n')) {
            break;
        }
        ;
    }
    return mp_obj_new_str_from_vstr(&out_buffer);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_readline_obj, 1, 2, mp_io_file_readline);

static mp_obj_t mp_io_file_seek(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_file_t *self = mp_io_file_get(args[0]);
    mp_int_t offset = mp_obj_get_int(args[1]);
    mp_int_t whence = n_args > 2 ? mp_obj_get_int(args[2]) : SEEK_SET;
    mp_obj_t ret = mp_os_lseek(MP_OBJ_NEW_SMALL_INT(self->fd), mp_obj_new_int(offset), MP_OBJ_NEW_SMALL_INT(whence));
    return ret;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_seek_obj, 2, 3, mp_io_file_seek);

static mp_obj_t mp_io_file_tell(mp_obj_t self_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    return mp_os_lseek(MP_OBJ_NEW_SMALL_INT(self->fd), MP_OBJ_NEW_SMALL_INT(0), MP_OBJ_NEW_SMALL_INT(SEEK_CUR));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_tell_obj, mp_io_file_tell);

static mp_obj_t mp_io_file_truncate(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_file_t *self = mp_io_file_get(args[0]);
    mp_obj_t fd = mp_obj_new_int(self->fd);
    mp_obj_t size;
    if ((n_args <= 1) || (args[1] == mp_const_none)) {
        size = mp_os_lseek(fd, MP_OBJ_NEW_SMALL_INT(0), MP_OBJ_NEW_SMALL_INT(SEEK_CUR));
    } else {
        size = args[1];
    }
    return mp_os_truncate(fd, size);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_truncate_obj, 1, 2, mp_io_file_truncate);

static mp_obj_t mp_io_file_writable(mp_obj_t self_in) {
    return mp_io_file_has_flags(self_in, FWRITE);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_writable_obj, mp_io_file_writable);

static mp_obj_t mp_io_file_write(mp_obj_t self_in, mp_obj_t b_in) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    if (mp_obj_is_str(b_in)) {
        mp_raise_TypeError(NULL);
    }
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(b_in, &bufinfo, MP_BUFFER_READ);
    int ret = mp_os_write_str(self->fd, bufinfo.buf, bufinfo.len);
    if (mp_os_nonblocking_ret(ret)) {
        return mp_const_none;
    }
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_file_write_obj, mp_io_file_write);

static mp_obj_t mp_io_exit(size_t n_args, const mp_obj_t *args) {
    mp_obj_t new_args[2];
    mp_load_method(args[0], MP_QSTR_close, new_args);
    return mp_call_method_n_kw(0, 0, new_args);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_exit_obj, 1, 4, mp_io_exit);

static mp_obj_t mp_io_iternext(mp_obj_t self_in) {
    mp_obj_t args[2];
    mp_load_method(self_in, MP_QSTR_readline, args);
    mp_obj_t ret = mp_call_method_n_kw(0, 0, args);
    return mp_obj_is_true(ret) ? ret : MP_OBJ_STOP_ITERATION;
}

static mp_obj_t mp_io_file_flush(mp_obj_t self_in) {
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_flush_obj, mp_io_file_flush);

static mp_obj_t mp_io_readlines(size_t n_args, const mp_obj_t *args) {
    mp_int_t hint = -1;
    if ((n_args > 1) && (args[1] != mp_const_none)) {
        hint = mp_obj_get_int(args[1]);
    }
    mp_obj_t list = mp_obj_new_list((hint >= 0) ? hint : 0, NULL);
    mp_obj_t readline_args[2];
    mp_load_method(args[0], MP_QSTR_readline, readline_args);
    while (hint) {
        mp_obj_t line = mp_call_method_n_kw(0, 0, readline_args);
        if (!mp_obj_is_true(line)) {
            break;
        }
        mp_obj_list_append(list, line);
        hint--;
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_readlines_obj, 1, 2, mp_io_readlines);

static mp_obj_t mp_io_seekable(mp_obj_t self_in) {
    mp_obj_t args[4];
    mp_load_method(self_in, MP_QSTR_seek, args);
    args[2] = MP_OBJ_NEW_SMALL_INT(0);
    args[3] = MP_OBJ_NEW_SMALL_INT(SEEK_CUR);
    int ret = 0;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_call_method_n_kw(2, 0, args);
        nlr_pop();
        ret = 1;
    }
    return mp_obj_new_bool(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_seekable_obj, mp_io_seekable);

static mp_obj_t mp_io_writelines(mp_obj_t self_in, mp_obj_t lines_in) {
    mp_obj_t args[3];
    mp_load_method(self_in, MP_QSTR_write, args);
    args[2] = mp_iternext(lines_in);
    while (args[2] != MP_OBJ_STOP_ITERATION) {
        mp_call_method_n_kw(1, 0, args);
        args[2] = mp_iternext(lines_in);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_writelines_obj, mp_io_writelines);

static mp_uint_t mp_io_file_stream_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    vstr_t vstr;
    vstr_init_fixed_buf(&vstr, size, buf);
    int ret = mp_os_read_vstr(self->fd, &vstr, size);
    if (ret < 0) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }
    return ret;
}

static mp_uint_t mp_io_file_stream_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_io_file_t *self = mp_io_file_get(self_in);
    int ret = mp_os_write_str(self->fd, buf, size);
    if (ret < 0) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }
    return ret;
}

static_assert(MP_SEEK_SET == SEEK_SET);
static_assert(MP_SEEK_CUR == SEEK_CUR);
static_assert(MP_SEEK_END == SEEK_END);

static const mp_rom_map_elem_t mp_io_file_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),     MP_ROM_PTR(&mp_io_file_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),   MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),    MP_ROM_PTR(&mp_io_exit_obj) },

    { MP_ROM_QSTR(MP_QSTR_close),       MP_ROM_PTR(&mp_io_file_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno),      MP_ROM_PTR(&mp_io_file_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),       MP_ROM_PTR(&mp_io_file_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),      MP_ROM_PTR(&mp_io_file_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),        MP_ROM_PTR(&mp_io_file_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readable),    MP_ROM_PTR(&mp_io_file_readable_obj) },
    { MP_ROM_QSTR(MP_QSTR_readall),     MP_ROM_PTR(&mp_io_file_readall_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),    MP_ROM_PTR(&mp_io_file_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline),    MP_ROM_PTR(&mp_io_file_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines),   MP_ROM_PTR(&mp_io_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek),        MP_ROM_PTR(&mp_io_file_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_seekable),    MP_ROM_PTR(&mp_io_seekable_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell),        MP_ROM_PTR(&mp_io_file_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR_truncate),    MP_ROM_PTR(&mp_io_file_truncate_obj) },
    { MP_ROM_QSTR(MP_QSTR_writable),    MP_ROM_PTR(&mp_io_file_writable_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),       MP_ROM_PTR(&mp_io_file_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_writelines),  MP_ROM_PTR(&mp_io_writelines_obj) },
};
static MP_DEFINE_CONST_DICT(mp_io_file_locals_dict, mp_io_file_locals_dict_table);

static mp_uint_t mp_io_stream_ioctl(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode);

static const mp_stream_p_t mp_io_file_stream_p = {
    .read = mp_io_file_stream_read,
    .write = mp_io_file_stream_write,
    .ioctl = mp_io_stream_ioctl,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_io_fileio,
    MP_QSTR_FileIO,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    make_new, mp_io_file_make_new,
    print, mp_io_file_print,
    attr, mp_io_file_attr,
    iter, &mp_io_iternext,
    protocol, &mp_io_file_stream_p,
    locals_dict, &mp_io_file_locals_dict
    );


// generic MicroPy stream protocol wrapper

enum none_mode {
    NONE_ZERO,          // None means return zero
    NONE_NONBLOCK,      // None means return non-blocking stream error
    NONE_ERROR,         // None means raise TypeError
};

static mp_uint_t mp_io_stream_call(size_t n_args, size_t n_kw, const mp_obj_t *args, int *errcode, enum none_mode none) {
    nlr_buf_t nlr;
    mp_obj_t ret_obj;
    if (nlr_push(&nlr) == 0) {
        ret_obj = mp_call_method_n_kw(n_args, n_kw, args);
        nlr_pop();
    } else if (mp_obj_is_os_error(nlr.ret_val, errcode)) {
        return MP_STREAM_ERROR;
    } else {
        nlr_raise(nlr.ret_val);
    }

    if (ret_obj == mp_const_none) {
        if (none == NONE_ZERO) {
            return 0;
        }
        if (none == NONE_NONBLOCK) {
            *errcode = MP_EAGAIN;
            return MP_STREAM_ERROR;
        }
    }
    return mp_obj_get_int(ret_obj);
}

static mp_uint_t mp_io_stream_read(mp_obj_t obj, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_t args[3];
    mp_load_method(obj, MP_QSTR_readinto, args);
    char *data = m_new(char, size);
    args[2] = mp_obj_new_bytearray_by_ref(size, data);

    mp_uint_t ret = mp_io_stream_call(1, 0, args, errcode, NONE_NONBLOCK);
    if (ret != MP_STREAM_ERROR) {
        memcpy(buf, data, ret);
    }
    return ret;
}

static mp_uint_t mp_io_stream_write(mp_obj_t obj, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_t args[3];
    mp_load_method(obj, MP_QSTR_write, args);
    char *data = m_new(char, size);
    args[2] = mp_obj_new_bytearray_by_ref(size, data);
    memcpy(data, buf, size);

    return mp_io_stream_call(1, 0, args, errcode, NONE_NONBLOCK);
}

static mp_uint_t mp_io_stream_ioctl(mp_obj_t obj, mp_uint_t request, uintptr_t arg, int *errcode) {
    switch (request) {
        case MP_STREAM_FLUSH: {
            mp_obj_t args[2];
            mp_load_method(obj, MP_QSTR_flush, args);
            return mp_io_stream_call(0, 0, args, errcode, NONE_ZERO);
        }
        case MP_STREAM_SEEK: {
            struct mp_stream_seek_t *s = (void *)arg;
            mp_obj_t args[4];
            mp_load_method(obj, MP_QSTR_seek, args);
            args[2] = mp_obj_new_int(s->offset);
            args[3] = MP_OBJ_NEW_SMALL_INT(s->whence);
            return mp_io_stream_call(2, 0, args, errcode, NONE_ERROR);
        }
        case MP_STREAM_CLOSE: {
            mp_obj_t args[2];
            mp_load_method(obj, MP_QSTR_close, args);
            return mp_io_stream_call(0, 0, args, errcode, NONE_ZERO);
        }
        case MP_STREAM_GET_FILENO: {
            mp_obj_t args[2];
            mp_load_method(obj, MP_QSTR_fileno, args);
            return mp_io_stream_call(0, 0, args, errcode, NONE_ERROR);
        }
        default: {
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
        }
    }
}

static const mp_stream_p_t mp_io_stream_p = {
    .read = mp_io_stream_read,
    .write = mp_io_stream_write,
    .ioctl = mp_io_stream_ioctl,
};

static const mp_stream_p_t *mp_io_get_stream(mp_obj_t obj) {
    const mp_stream_p_t *mp_stream = mp_get_stream(obj);
    return mp_stream ? mp_stream : &mp_io_stream_p;
}


// TextIOWrapper

static mp_obj_io_text_t *mp_io_text_get(mp_obj_t self_in) {
    // mp_obj_io_text_t *self = MP_OBJ_TO_PTR(self_in);
    mp_obj_io_text_t *self = MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&mp_type_io_textio)));
    if (self->stream == MP_OBJ_NULL) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed file"));
    }
    return self;
}

static void mp_io_init_ring(ring_t *ring) {
    if (!ring->buffer) {
        ring->buffer = m_malloc(MP_OS_DEFAULT_BUFFER_SIZE);
        ring->size = MP_OS_DEFAULT_BUFFER_SIZE;
        ring->read_index = 0;
        ring->write_index = 0;
    }
}

static void mp_io_deinit_ring(ring_t *ring) {
    memset(ring, 0, sizeof(ring_t));
}

static size_t mp_io_text_find_newline(ring_t *ring) {
    size_t cr = ring_chr(ring, '\r');
    size_t nl = ring_chr(ring, '\n');
    return (nl <= cr + 1) ? nl : cr;
}

#define MAX_UTF8_BYTES 4

static size_t mp_io_text_decode(vstr_t *vstr, ring_t *ring, size_t max_codepoints, size_t max_bytes, bool flush) {
    size_t num_codepoints = 0;  // total number of code points decoded
    size_t num_bytes = 0;  // total number of bytes read
    while ((num_codepoints < max_codepoints) && (num_bytes < max_bytes)) {
        char src[MAX_UTF8_BYTES];
        size_t len = ring_read(ring, src, MIN(MAX_UTF8_BYTES, max_bytes - num_bytes));
        if (!len) {
            break;
        }

        size_t n = 0;  // parsing an n-byte code point sequence
        // at the start of a new sequence
        if (src[0] < 0x80) {
            // ASCII code point (1 byte sequence)
            n = 1;
        } else if (src[0] < 0xC2) {
            // error
        } else if (src[0] < 0xE0) {
            // 2 byte sequence
            n = 2;
        } else if (src[0] < 0xF0) {
            // 3 byte sequence
            n = 3;
        } else if (src[0] < 0xF5) {
            // 4 byte sequence
            n = 4;
        } else {
            // error;
        }

        size_t i = 1;  // at the ith byte of an n-byte code point sequence
        while (i < n) {
            if (i < len) {
            } else if (flush) {
                // unterminated sequence is an error
                n = 0;
                break;
            } else {
                // incomplete sequence means abort
                ring->read_index += -len;
                return num_codepoints;
            }
            // in the middle of a sequence
            if (src[i] < 0x80) {
                // error
                n = 0;
            } else if (src[i] < 0xC0) {
                // continuation byte
            } else {
                // error
                n = 0;
            }
        }

        if (n == 0) {
            // error occurred, emit surrogate escape
            char *dst = vstr_add_len(vstr, 2);
            dst[0] = 0xDC;
            dst[1] = 0x80 | (src[0] & 0x7F);
            num_codepoints++;
            ring->read_index += 1 - len;
        } else if (i == n) {
            // emit valid utf-8 sequence
            vstr_add_strn(vstr, src, n);
            num_codepoints++;
            ring->read_index += n - len;
        }
    }
    return num_codepoints;
}

static int mp_io_text_fill_ring(mp_obj_io_text_t *self) {
    const mp_stream_p_t *stream = mp_io_get_stream(self->stream);
    ring_t *ring = &self->in_buffer;
    mp_io_init_ring(ring);
    size_t size;
    char *write_ptr = ring_at(ring, ring->write_index, &size);
    size = MIN(size, ring_write_count(ring));
    int errcode;
    mp_uint_t ret = stream->read(self->stream, write_ptr, size, &errcode);
    if (ret != MP_STREAM_ERROR) {
        ring->write_index += ret;
        return ret;
    } else if (mp_is_nonblocking_error(errcode)) {
        return -1;
    } else {
        mp_io_deinit_ring(ring);
        mp_raise_OSError(errcode);
    }
}

static mp_obj_t mp_io_text_call(mp_obj_t self_in, qstr attr, size_t n_args, ...) {
    mp_obj_io_text_t *self = mp_io_text_get(self_in);
    assert(2 + n_args <= 4);
    mp_obj_t args[4];
    va_list ap;
    va_start(ap, n_args);
    mp_load_method(self->stream, attr, args);
    for (size_t i = 0; i < n_args; i++) {
        args[2 + i] = va_arg(ap, mp_obj_t);
    }
    va_end(ap);
    return mp_call_method_n_kw(n_args, 0, args);
}

// static mp_obj_t mp_io_text_load_attr(mp_obj_io_text_t *self, qstr attr) {
//     mp_obj_t args[2];
//     mp_load_method_maybe(self->stream, attr, args);
//     return ((args[0] != MP_OBJ_NULL) && (args[1] == MP_OBJ_NULL)) ? args[0] : MP_OBJ_NULL;
// }

static mp_obj_t mp_io_text_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_buffer, MP_QSTR_encoding, MP_QSTR_errors, MP_QSTR_newline, MP_QSTR_line_buffering, 0 };
    mp_obj_t stream;
    parse_args_and_kw(n_args, n_kw, args, "O|OOOp", kws, &stream, NULL, NULL, NULL, NULL);

    mp_obj_io_text_t *self = mp_obj_malloc_with_finaliser(mp_obj_io_text_t, type);
    self->stream = stream;
    self->isatty = mp_obj_is_true(mp_io_text_call(self, MP_QSTR_isatty, 0));
    return MP_OBJ_FROM_PTR(self);
}

static void mp_io_text_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    mp_obj_io_text_t *self = mp_io_text_get(self_in);
    mp_printf(print, "<io.%q>", (qstr)self->base.type->name);

    // mp_print_str(print, "<io.");
    // mp_obj_print_helper(print, MP_OBJ_NEW_QSTR(self->base.type->name), PRINT_STR);

    // mp_obj_t name = mp_io_text_load_attr(self, MP_QSTR_name);
    // if (name) {
    //     mp_print_str(print, " name='");
    //     mp_obj_print_helper(print, name, PRINT_STR);
    //     mp_print_str(print, "'");
    // }

    // mp_obj_t mode = mp_io_text_load_attr(self, MP_QSTR_mode);
    // if (mode) {
    //     mp_print_str(print, " mode='");
    //     mp_obj_print_helper(print, mode, PRINT_STR);
    //     mp_print_str(print, "'");
    // }

    // mp_print_str(print, ">");
}

static void mp_io_text_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_obj_io_text_t *self = MP_OBJ_TO_PTR(self_in);
    if (dest[0] == MP_OBJ_SENTINEL) {
        return;
    }
    switch (attr) {
        case MP_QSTR_buffer:
            mp_io_text_get(self_in);
            dest[0] = self->stream;
            break;
        case MP_QSTR_closed:
            dest[0] = mp_obj_new_bool(self->stream == MP_OBJ_NULL);
            break;
        default:
            dest[1] = MP_OBJ_SENTINEL;
            break;
    }
}

static mp_obj_t mp_io_text_close(mp_obj_t self_in) {
    mp_obj_io_text_t *self = MP_OBJ_TO_PTR(self_in);
    mp_io_deinit_ring(&self->in_buffer);
    if (self->stream) {
        mp_io_text_call(self_in, MP_QSTR_close, 0);
        self->stream = MP_OBJ_NULL;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_close_obj, mp_io_text_close);

static mp_obj_t mp_io_text_detach(mp_obj_t self_in) {
    mp_obj_io_text_t *self = mp_io_text_get(self_in);
    mp_io_deinit_ring(&self->in_buffer);
    mp_obj_t ret_obj = self->stream;
    self->stream = MP_OBJ_NULL;
    return ret_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_detach_obj, mp_io_text_detach);

static mp_obj_t mp_io_text_fileno(mp_obj_t self_in) {
    return mp_io_text_call(self_in, MP_QSTR_fileno, 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_fileno_obj, mp_io_text_fileno);

static mp_obj_t mp_io_text_flush(mp_obj_t self_in) {
    return mp_io_text_call(self_in, MP_QSTR_flush, 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_flush_obj, mp_io_text_flush);

static mp_obj_t mp_io_text_isatty(mp_obj_t self_in) {
    return mp_io_text_call(self_in, MP_QSTR_isatty, 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_isatty_obj, mp_io_text_isatty);

static mp_obj_t mp_io_text_readall(mp_obj_t self_in);

static mp_obj_t mp_io_text_read(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_text_t *self = mp_io_text_get(args[0]);
    mp_int_t opt_size = n_args > 1 ? mp_obj_get_int(args[1]) : -1;
    if (opt_size < 0) {
        return mp_io_text_readall(args[0]);
    }

    size_t size = opt_size;
    vstr_t out_buffer;
    vstr_init(&out_buffer, size);
    int ret = 1;
    size_t num_codepoints = 0;
    while (ret >= 0) {
        num_codepoints += mp_io_text_decode(&out_buffer, &self->in_buffer, size, -1, ret == 0);
        if ((ret == 0) || (num_codepoints > 0)) {
            break;
        }
        ret = mp_io_text_fill_ring(self);
    }
    return mp_obj_new_str_from_vstr(&out_buffer);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_text_read_obj, 1, 2, mp_io_text_read);

static mp_obj_t mp_io_text_readable(mp_obj_t self_in) {
    return mp_io_text_call(self_in, MP_QSTR_readable, 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_readable_obj, mp_io_text_readable);

static mp_obj_t mp_io_text_readall(mp_obj_t self_in) {
    mp_obj_io_text_t *self = mp_io_text_get(self_in);

    vstr_t out_buffer;
    vstr_init(&out_buffer, MP_OS_DEFAULT_BUFFER_SIZE);
    int ret = 1;
    while (ret >= 0) {
        mp_io_text_decode(&out_buffer, &self->in_buffer, -1, -1, ret == 0);
        if (ret == 0) {
            break;
        }
        ret = mp_io_text_fill_ring(self);
    }
    return mp_obj_new_str_from_vstr(&out_buffer);
}

static mp_obj_t mp_io_text_readline(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_text_t *self = mp_io_text_get(args[0]);
    size_t size = (n_args > 1) ? mp_obj_get_int(args[1]) : -1;

    vstr_t out_buffer;
    vstr_init(&out_buffer, MIN(size, MP_OS_DEFAULT_BUFFER_SIZE));
    int ret = 1;
    size_t num_codepoints = 0;
    ring_t *ring = &self->in_buffer;
    while (ret >= 0) {
        size_t nl_index = mp_io_text_find_newline(ring);
        num_codepoints += mp_io_text_decode(&out_buffer, ring, size - num_codepoints, nl_index - ring->read_index, ret == 0);
        if ((nl_index < ring->write_index) || (ret == 0) || (num_codepoints >= size)) {
            // if found NL || read EOF || got max read size
            break;
        }
        ret = mp_io_text_fill_ring(self);
    }
    return mp_obj_new_str_from_vstr(&out_buffer);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_text_readline_obj, 1, 2, mp_io_text_readline);

static mp_obj_t mp_io_text_seek(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_text_t *self = mp_io_text_get(args[0]);
    off_t pos = mp_obj_get_int(args[1]);
    pos -= ring_read_count(&self->in_buffer);
    mp_obj_t ret_obj = mp_io_text_call(args[0], MP_QSTR_seek, n_args - 1, mp_obj_new_int(pos), (n_args > 2) ? args[2] : MP_OBJ_NULL);
    if (!self->isatty) {
        ring_clear(&self->in_buffer);
    }
    return ret_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_text_seek_obj, 2, 3, mp_io_text_seek);

static mp_obj_t mp_io_text_seekable(mp_obj_t self_in) {
    return mp_io_text_call(self_in, MP_QSTR_seekable, 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_seekable_obj, mp_io_text_seekable);

static mp_obj_t mp_io_text_tell(mp_obj_t self_in) {
    mp_obj_io_text_t *self = mp_io_text_get(self_in);
    mp_obj_t pos_obj = mp_io_text_call(self_in, MP_QSTR_tell, 0);
    off_t pos = mp_obj_get_int(pos_obj);
    pos -= ring_read_count(&self->in_buffer);
    return mp_obj_new_int(pos);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_tell_obj, mp_io_text_tell);

static mp_obj_t mp_io_text_truncate(size_t n_args, const mp_obj_t *args) {
    mp_obj_io_text_t *self = mp_io_text_get(args[0]);
    mp_obj_t ret_obj = mp_io_text_call(args[0], MP_QSTR_truncate, n_args - 1, (n_args > 1) ? args[1] : MP_OBJ_NULL);
    if (!self->isatty) {
        ring_clear(&self->in_buffer);
    }
    return ret_obj;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_text_truncate_obj, 1, 2, mp_io_text_truncate);

static mp_obj_t mp_io_text_writable(mp_obj_t self_in) {
    return mp_io_text_call(self_in, MP_QSTR_writable, 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_io_text_writable_obj, mp_io_text_writable);

static size_t mp_io_text_rwrite(mp_obj_io_text_t *self, const char *src, size_t len) {
    const mp_stream_p_t *stream = mp_io_get_stream(self->stream);
    int errcode;
    mp_uint_t ret = stream->write(self->stream, src, len, &errcode);
    if (ret == MP_STREAM_ERROR) {
        mp_raise_OSError(errcode);
    }
    return ret;
}

static mp_obj_t mp_io_text_write(mp_obj_t self_in, mp_obj_t b_in) {
    mp_obj_io_text_t *self = mp_io_text_get(self_in);
    size_t b_len;
    const char *b_str = mp_obj_str_get_data(b_in, &b_len);
    size_t num_codepoints = mp_io_text_rwrite(self, b_str, b_len);
    return mp_obj_new_int(num_codepoints);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_io_text_write_obj, mp_io_text_write);

void mp_io_print(void *data, const char *str, size_t len) {
    bool is_textio = mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(mp_obj_get_type(MP_OBJ_FROM_PTR(data))), MP_OBJ_FROM_PTR(&mp_type_io_textio));
    if (is_textio) {
        mp_obj_io_text_t *self = data;
        mp_io_text_rwrite(self, str, len);
    } else {
        mp_obj_t args[3];
        mp_load_method(MP_OBJ_FROM_PTR(data), MP_QSTR_write, args);
        args[2] = mp_obj_new_str_copy(&mp_type_str, (const byte *)str, len);
        mp_call_method_n_kw(1, 0, args);
    }
}

static const mp_rom_map_elem_t mp_io_text_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),     MP_ROM_PTR(&mp_io_text_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),   MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),    MP_ROM_PTR(&mp_io_exit_obj) },

    { MP_ROM_QSTR(MP_QSTR_close),       MP_ROM_PTR(&mp_io_text_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_detach),      MP_ROM_PTR(&mp_io_text_detach_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno),      MP_ROM_PTR(&mp_io_text_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),       MP_ROM_PTR(&mp_io_text_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),      MP_ROM_PTR(&mp_io_text_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),        MP_ROM_PTR(&mp_io_text_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readable),    MP_ROM_PTR(&mp_io_text_readable_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline),    MP_ROM_PTR(&mp_io_text_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines),   MP_ROM_PTR(&mp_io_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek),        MP_ROM_PTR(&mp_io_text_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_seekable),    MP_ROM_PTR(&mp_io_text_seekable_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell),        MP_ROM_PTR(&mp_io_text_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR_truncate),    MP_ROM_PTR(&mp_io_text_truncate_obj) },
    { MP_ROM_QSTR(MP_QSTR_writable),    MP_ROM_PTR(&mp_io_text_writable_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),       MP_ROM_PTR(&mp_io_text_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_writelines),  MP_ROM_PTR(&mp_io_writelines_obj) },
};
static MP_DEFINE_CONST_DICT(mp_io_text_locals_dict, mp_io_text_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_io_textio,
    MP_QSTR_TextIOWrapper,
    MP_TYPE_FLAG_ITER_IS_ITERNEXT,
    make_new, mp_io_text_make_new,
    print, mp_io_text_print,
    attr, mp_io_text_attr,
    iter, mp_io_iternext,
    locals_dict, &mp_io_text_locals_dict
    );


// sys stdio

static mp_obj_io_file_t mp_sys_stdin_file_obj = {
    .base = { &mp_type_io_fileio },
    .fd = STDIN_FILENO,
    .name = MP_ROM_QSTR(MP_QSTR_stdin),
    .mode = MP_ROM_QSTR(MP_QSTR_r),
    .closefd = 0,
};
mp_obj_io_text_t mp_sys_stdin_obj = {
    .base = { &mp_type_io_textio },
    .stream = MP_ROM_PTR(&mp_sys_stdin_file_obj),
    .isatty = 1,
};

static mp_obj_io_file_t mp_sys_stdout_file_obj = {
    .base = { &mp_type_io_fileio },
    .fd = STDOUT_FILENO,
    .name = MP_ROM_QSTR(MP_QSTR_stdout),
    .mode = MP_ROM_QSTR(MP_QSTR_w),
    .closefd = 0,
};
mp_obj_io_text_t mp_sys_stdout_obj = {
    .base = { &mp_type_io_textio },
    .stream = MP_ROM_PTR(&mp_sys_stdout_file_obj),
    .isatty = 1,
};

static mp_obj_io_file_t mp_sys_stderr_file_obj = {
    .base = { &mp_type_io_fileio },
    .fd = STDERR_FILENO,
    .name = MP_ROM_QSTR(MP_QSTR_stderr),
    .mode = MP_ROM_QSTR(MP_QSTR_w),
    .closefd = 0,
};
mp_obj_io_text_t mp_sys_stderr_obj = {
    .base = { &mp_type_io_textio },
    .stream = MP_ROM_PTR(&mp_sys_stderr_file_obj),
    .isatty = 1,
};


// module

static const mp_rom_map_elem_t mp_module_io_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_io) },
    { MP_ROM_QSTR(MP_QSTR_open),        MP_ROM_PTR(&mp_builtin_open_obj) },

    { MP_ROM_QSTR(MP_QSTR_StringIO),    MP_ROM_PTR(&mp_type_stringio) },
    #if MICROPY_PY_IO_BYTESIO
    { MP_ROM_QSTR(MP_QSTR_BytesIO),     MP_ROM_PTR(&mp_type_bytesio) },
    #endif

    { MP_ROM_QSTR(MP_QSTR_FileIO),      MP_ROM_PTR(&mp_type_io_fileio) },
    { MP_ROM_QSTR(MP_QSTR_TextIOWrapper), MP_ROM_PTR(&mp_type_io_textio) },

    { MP_ROM_QSTR(MP_QSTR_DEFAULT_BUFFER_SIZE), MP_ROM_INT(MP_OS_DEFAULT_BUFFER_SIZE) },
};

static MP_DEFINE_CONST_DICT(mp_module_io_globals, mp_module_io_globals_table);

const mp_obj_module_t mp_module_io = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_io_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_io, mp_module_io);
