// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <errno.h>

#include "py/obj.h"


#ifndef MP_OS_DEFAULT_BUFFER_SIZE
#define MP_OS_DEFAULT_BUFFER_SIZE 256
#endif

#define MP_OS_CALL(ret, func, ...) for (;;) { \
        MP_THREAD_GIL_EXIT(); \
        ret = func(__VA_ARGS__); \
        MP_THREAD_GIL_ENTER(); \
        if ((ret >= 0) || (errno != EINTR)) { \
            break; \
        } \
        else { \
            mp_handle_pending(true); \
        } \
}

#define MP_OS_CALL_NULL(ret, func, ...) for (;;) { \
        MP_THREAD_GIL_EXIT(); \
        ret = func(__VA_ARGS__); \
        MP_THREAD_GIL_ENTER(); \
        if ((ret != NULL) || (errno != EINTR)) { \
            break; \
        } \
        else { \
            mp_handle_pending(true); \
        } \
}

mp_obj_t mp_os_check_ret(int ret);

bool mp_os_nonblocking_ret(int ret);

int mp_os_get_fd(mp_obj_t obj_in);

MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_open_obj);

int mp_os_read_vstr(int fd, vstr_t *vstr, size_t size);

int mp_os_write_str(int fd, const char *str, size_t len);
