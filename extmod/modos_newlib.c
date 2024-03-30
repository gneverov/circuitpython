// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-FileCopyrightText: 2016-2022 Damien P. George
// SPDX-License-Identifier: MIT

#include "py/builtin.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "py/stream.h"

#if MICROPY_PY_OS
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/times.h>
#include "newlib/dirent.h"
#include "newlib/dlfcn.h"
#include "newlib/mount.h"
#include "newlib/newlib.h"
#include "newlib/random.h"
#include "newlib/statvfs.h"
#include "newlib/unistd.h"

#if MICROPY_PY_OS_UNAME
#include "genhdr/mpversion.h"
#endif

#ifdef MICROPY_PY_OS_INCLUDEFILE
#include MICROPY_PY_OS_INCLUDEFILE
#endif

#ifdef MICROPY_BUILD_TYPE
#define MICROPY_BUILD_TYPE_PAREN " (" MICROPY_BUILD_TYPE ")"
#else
#define MICROPY_BUILD_TYPE_PAREN
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

STATIC mp_obj_t mp_os_check_ret(int ret) {
    if (ret >= 0) {
        return mp_obj_new_int(ret);
    } else {
        mp_raise_OSError(errno);
    }
}


#if MICROPY_PY_OS_UNAME

#if MICROPY_PY_OS_UNAME_RELEASE_DYNAMIC
#define CONST_RELEASE
#else
#define CONST_RELEASE const
#endif

STATIC const qstr mp_os_uname_info_fields[] = {
    MP_QSTR_sysname,
    MP_QSTR_nodename,
    MP_QSTR_release,
    MP_QSTR_version,
    MP_QSTR_machine
};
STATIC const MP_DEFINE_STR_OBJ(mp_os_uname_info_sysname_obj, MICROPY_PY_SYS_PLATFORM);
STATIC const MP_DEFINE_STR_OBJ(mp_os_uname_info_nodename_obj, MICROPY_PY_SYS_PLATFORM);
STATIC CONST_RELEASE MP_DEFINE_STR_OBJ(mp_os_uname_info_release_obj, MICROPY_VERSION_STRING);
STATIC const MP_DEFINE_STR_OBJ(mp_os_uname_info_version_obj, MICROPY_GIT_TAG " on " MICROPY_BUILD_DATE MICROPY_BUILD_TYPE_PAREN);
STATIC const MP_DEFINE_STR_OBJ(mp_os_uname_info_machine_obj, MICROPY_HW_BOARD_NAME " with " MICROPY_HW_MCU_NAME);

STATIC MP_DEFINE_ATTRTUPLE(
    mp_os_uname_info_obj,
    mp_os_uname_info_fields,
    5,
    MP_ROM_PTR(&mp_os_uname_info_sysname_obj),
    MP_ROM_PTR(&mp_os_uname_info_nodename_obj),
    MP_ROM_PTR(&mp_os_uname_info_release_obj),
    MP_ROM_PTR(&mp_os_uname_info_version_obj),
    MP_ROM_PTR(&mp_os_uname_info_machine_obj)
    );

STATIC mp_obj_t mp_os_uname(void) {
    #if MICROPY_PY_OS_UNAME_RELEASE_DYNAMIC
    const char *release = mp_os_uname_release();
    mp_os_uname_info_release_obj.len = strlen(release);
    mp_os_uname_info_release_obj.data = (const byte *)release;
    #endif
    return MP_OBJ_FROM_PTR(&mp_os_uname_info_obj);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_os_uname_obj, mp_os_uname);

#endif

// Process Parameters
// ------------------
STATIC mp_obj_t mp_os_environ(void) {
    mp_obj_t dict = mp_obj_new_dict(0);
    extern char **environ;
    char **env = environ;
    while (*env) {
        char *equal = strchr(*env, '=');
        if (equal) {
            mp_obj_t key = mp_obj_new_str(*env, equal - *env);
            mp_obj_t value = mp_obj_new_str(equal + 1, strlen(equal) - 1);
            mp_obj_dict_store(dict, key, value);
        }
        env++;
    }
    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_os_environ_obj, mp_os_environ);

STATIC mp_obj_t mp_os_getenv(mp_obj_t key_in) {
    const char *key = mp_obj_str_get_str(key_in);
    char *value = getenv(key);
    if (!value) {
        return mp_const_none;
    }
    size_t len = strlen(value);
    return mp_obj_new_str(value, len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_getenv_obj, mp_os_getenv);

STATIC mp_obj_t mp_os_getpid(void) {
    int pid = getpid();
    return mp_obj_new_int(pid);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_os_getpid_obj, mp_os_getpid);

STATIC mp_obj_t mp_os_putenv(mp_obj_t key_in, mp_obj_t value_in) {
    const char *key = mp_obj_str_get_str(key_in);
    const char *value = mp_obj_str_get_str(value_in);
    int ret = setenv(key, value, 1);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_os_putenv_obj, mp_os_putenv);

STATIC mp_obj_t mp_os_strerror(mp_obj_t code_in) {
    mp_int_t code = mp_obj_get_int(code_in);
    char *s = strerror(code);
    if (!s) {
        mp_raise_ValueError(NULL);
    }
    return mp_obj_new_str(s, strlen(s));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_strerror_obj, mp_os_strerror);

STATIC mp_obj_t mp_os_unsetenv(mp_obj_t key_in) {
    const char *key = mp_obj_str_get_str(key_in);
    int ret = unsetenv(key);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_unsetenv_obj, mp_os_unsetenv);

// File Descriptor Operations
// --------------------------
STATIC mp_obj_t mp_os_close(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret;
    MP_OS_CALL(ret, close, fd);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_close_obj, mp_os_close);

STATIC mp_obj_t mp_os_dup(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret;
    MP_OS_CALL(ret, dup, fd);
    return mp_os_check_ret(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_dup_obj, mp_os_dup);

STATIC mp_obj_t mp_os_dup2(mp_obj_t fd1_in, mp_obj_t fd2_in) {
    mp_int_t fd1 = mp_obj_get_int(fd1_in);
    mp_int_t fd2 = mp_obj_get_int(fd2_in);
    int ret;
    MP_OS_CALL(ret, dup2, fd1, fd2);
    return mp_os_check_ret(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_os_dup2_obj, mp_os_dup2);

STATIC mp_obj_t mp_os_fsync(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret;
    MP_OS_CALL(ret, fsync, fd);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_fsync_obj, mp_os_fsync);

STATIC mp_obj_t mp_os_isatty(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret;
    MP_OS_CALL(ret, isatty, fd);
    mp_os_check_ret(ret);
    return mp_obj_new_bool(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_isatty_obj, mp_os_isatty);

STATIC mp_obj_t mp_os_lseek(mp_obj_t fd_in, mp_obj_t pos_in, mp_obj_t whence_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    mp_int_t pos = mp_obj_get_int(pos_in);
    mp_int_t whence = mp_obj_get_int(whence_in);
    int ret;
    MP_OS_CALL(ret, lseek, fd, pos, whence);
    return mp_os_check_ret(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mp_os_lseek_obj, mp_os_lseek);

STATIC mp_obj_t mp_os_open(size_t n_args, const mp_obj_t *args) {
    const char *path = mp_obj_str_get_str(args[0]);
    mp_int_t flags = mp_obj_get_int(args[1]);
    mp_int_t mode = n_args > 2 ? mp_obj_get_int(args[2]) : 0777;
    int ret;
    MP_OS_CALL(ret, open, path, flags, mode);
    return mp_os_check_ret(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_open_obj, 2, 3, mp_os_open);

STATIC mp_obj_t mp_os_read(mp_obj_t fd_in, mp_obj_t n_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    mp_int_t n = mp_obj_get_int(n_in);
    vstr_t buf;
    vstr_init(&buf, n);
    int ret;
    MP_OS_CALL(ret, read, fd, vstr_str(&buf), n);
    mp_os_check_ret(ret);
    vstr_add_len(&buf, ret);
    return mp_obj_new_bytes_from_vstr(&buf);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_os_read_obj, mp_os_read);

STATIC mp_obj_t mp_os_write(mp_obj_t fd_in, mp_obj_t str_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    size_t len;
    const char *str = mp_obj_str_get_data(str_in, &len);
    int ret;
    MP_OS_CALL(ret, write, fd, str, len);
    return mp_os_check_ret(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_os_write_obj, mp_os_write);

// Files and Directories
// ---------------------
STATIC mp_obj_t mp_os_chdir(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret;
    MP_OS_CALL(ret, chdir, path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_chdir_obj, mp_os_chdir);

STATIC mp_obj_t mp_os_getcwd(void) {
    vstr_t buf;
    vstr_init(&buf, 256);
    const char *cwd = getcwd(vstr_str(&buf), 256);
    vstr_add_len(&buf, strnlen(cwd, 256));
    return mp_obj_new_str_from_vstr(&buf);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_os_getcwd_obj, mp_os_getcwd);

STATIC mp_obj_t mp_os_scandir(size_t n_args, const mp_obj_t *args);

STATIC mp_obj_t mp_os_listdir(size_t n_args, const mp_obj_t *args) {
    mp_obj_t iter = mp_os_scandir(n_args, args);
    mp_obj_t list = mp_obj_new_list(0, NULL);
    mp_obj_t next = mp_iternext(iter);
    while (next != MP_OBJ_STOP_ITERATION) {
        size_t len;
        mp_obj_t *items;
        mp_obj_tuple_get(next, &len, &items);
        mp_obj_list_append(list, items[0]);
        next = mp_iternext(iter);
    }
    return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_listdir_obj, 0, 1, mp_os_listdir);

STATIC mp_obj_t mp_os_mkdir(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret;
    MP_OS_CALL(ret, mkdir, path, 0777);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_mkdir_obj, mp_os_mkdir);

STATIC mp_obj_t mp_os_rename(mp_obj_t src_in, mp_obj_t dst_in) {
    const char *src = mp_obj_str_get_str(src_in);
    const char *dst = mp_obj_str_get_str(dst_in);
    int ret;
    MP_OS_CALL(ret, rename, src, dst);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_os_rename_obj, mp_os_rename);

STATIC mp_obj_t mp_os_rmdir(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret;
    MP_OS_CALL(ret, rmdir, path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_rmdir_obj, mp_os_rmdir);

typedef struct {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    mp_fun_1_t finaliser;
    const mp_obj_type_t *type;
    DIR *dirp;
} mp_os_scandir_iter_t;

STATIC mp_obj_t mp_os_scandir_iter_del(mp_obj_t self_in) {
    mp_os_scandir_iter_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->dirp) {
        int ret;
        MP_OS_CALL(ret, closedir, self->dirp);
        self->dirp = NULL;
    }
    return mp_const_none;
}

STATIC mp_obj_t mp_os_scandir_iter_next(mp_obj_t self_in) {
    mp_os_scandir_iter_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->dirp) {
        return MP_OBJ_STOP_ITERATION;
    }

    errno = 0;
    struct dirent *dp;
    MP_OS_CALL_NULL(dp, readdir, self->dirp);
    if (dp) {
        static const qstr mp_os_direntry_attrs[] = { MP_QSTR_name };
        mp_obj_t items[] = {
            mp_obj_new_str_copy(self->type, (byte *)dp->d_name, strlen(dp->d_name)),
        };
        return mp_obj_new_attrtuple(mp_os_direntry_attrs, 1, items);
    }
    int orig_errno = errno;
    mp_os_scandir_iter_del(self_in);
    if (orig_errno != 0) {
        mp_raise_OSError(orig_errno);
    } else {
        return MP_OBJ_STOP_ITERATION;
    }
}

STATIC mp_obj_t mp_os_scandir(size_t n_args, const mp_obj_t *args) {
    const char *path = ".";
    const mp_obj_type_t *type = &mp_type_str;
    if (n_args > 0) {
        path = mp_obj_str_get_str(args[0]);
        type = mp_obj_get_type(args[0]);
    }
    mp_os_scandir_iter_t *iter = m_new_obj_with_finaliser(mp_os_scandir_iter_t);
    iter->base.type = &mp_type_polymorph_iter_with_finaliser;
    iter->iternext = mp_os_scandir_iter_next;
    iter->finaliser = mp_os_scandir_iter_del;
    iter->type = type;

    MP_OS_CALL_NULL(iter->dirp, opendir, path);
    if (!iter->dirp) {
        mp_raise_OSError(errno);
    }
    return MP_OBJ_FROM_PTR(iter);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_scandir_obj, 0, 1, mp_os_scandir);

STATIC mp_obj_t mp_os_stat_result(const struct stat *sb) {
    static const qstr mp_os_stat_attrs[] = {
        MP_QSTR_st_mode,
        MP_QSTR_st_ino,
        MP_QSTR_st_dev,
        MP_QSTR_st_nlink,
        MP_QSTR_st_uid,
        MP_QSTR_st_gid,
        MP_QSTR_st_size,
        MP_QSTR_st_atime,
        MP_QSTR_st_mtime,
        MP_QSTR_st_ctime,
    };
    mp_obj_t items[] = {
        mp_obj_new_int_from_uint(sb->st_mode),
        mp_obj_new_int_from_uint(sb->st_ino),
        mp_obj_new_int_from_uint(sb->st_dev),
        mp_obj_new_int_from_uint(sb->st_nlink),
        mp_obj_new_int_from_uint(sb->st_uid),
        mp_obj_new_int_from_uint(sb->st_gid),
        mp_obj_new_int_from_uint(sb->st_size),
        mp_obj_new_int_from_uint(sb->st_atime),
        mp_obj_new_int_from_uint(sb->st_mtime),
        mp_obj_new_int_from_uint(sb->st_ctime),
    };
    return mp_obj_new_attrtuple(mp_os_stat_attrs, 10, items);
}

STATIC mp_obj_t mp_os_stat(mp_obj_t path_in) {
    int ret;
    struct stat sb;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        MP_OS_CALL(ret, fstat, fd, &sb);
    } else {
        const char *path = mp_obj_str_get_str(path_in);
        MP_OS_CALL(ret, stat, path, &sb);
    }
    mp_os_check_ret(ret);
    return mp_os_stat_result(&sb);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_stat_obj, mp_os_stat);

STATIC mp_obj_t mp_os_statvfs_result(const struct statvfs *sb) {
    static const qstr mp_os_statvfs_attrs[] = {
        MP_QSTR_f_bsize,
        MP_QSTR_f_frsize,
        MP_QSTR_f_blocks,
        MP_QSTR_f_bfree,
        MP_QSTR_f_bavail,
        MP_QSTR_f_files,
        MP_QSTR_f_ffree,
        MP_QSTR_f_favail,
        MP_QSTR_f_flag,
        MP_QSTR_f_namemax,
    };
    mp_obj_t items[] = {
        mp_obj_new_int_from_uint(sb->f_bsize),
        mp_obj_new_int_from_uint(sb->f_frsize),
        mp_obj_new_int_from_uint(sb->f_blocks),
        mp_obj_new_int_from_uint(sb->f_bfree),
        mp_obj_new_int_from_uint(sb->f_bavail),
        mp_obj_new_int_from_uint(sb->f_files),
        mp_obj_new_int_from_uint(sb->f_ffree),
        mp_obj_new_int_from_uint(sb->f_favail),
        mp_obj_new_int_from_uint(sb->f_flag),
        mp_obj_new_int_from_uint(sb->f_namemax),
    };
    return mp_obj_new_attrtuple(mp_os_statvfs_attrs, 10, items);
}

STATIC mp_obj_t mp_os_statvfs(mp_obj_t path_in) {
    int ret;
    struct statvfs sb;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        MP_OS_CALL(ret, fstatvfs, fd, &sb);
    } else {
        const char *path = mp_obj_str_get_str(path_in);
        MP_OS_CALL(ret, statvfs, path, &sb);
    }
    mp_os_check_ret(ret);
    return mp_os_statvfs_result(&sb);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_statvfs_obj, mp_os_statvfs);

STATIC mp_obj_t mp_os_sync(void) {
    MP_THREAD_GIL_EXIT();
    sync();
    MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_os_sync_obj, mp_os_sync);

STATIC mp_obj_t mp_os_truncate(mp_obj_t path_in, mp_obj_t length_in) {
    int ret;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        mp_int_t length = mp_obj_get_int(length_in);
        MP_OS_CALL(ret, ftruncate, fd, length);
    } else {
        const char *path = mp_obj_str_get_str(path_in);
        mp_int_t length = mp_obj_get_int(length_in);
        MP_OS_CALL(ret, truncate, path, length);
    }
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_os_truncate_obj, mp_os_truncate);

STATIC mp_obj_t mp_os_unlink(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret;
    MP_OS_CALL(ret, unlink, path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_unlink_obj, mp_os_unlink);

// Process Management
// ------------------
STATIC mp_obj_t mp_os_abort(void) {
    abort();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_os_abort_obj, mp_os_abort);

STATIC mp_obj_t mp_os__exit(mp_obj_t n_in) {
    mp_int_t n = mp_obj_get_int(n_in);
    _exit(n);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os__exit_obj, mp_os__exit);

STATIC mp_obj_t mp_os_kill(mp_obj_t pid_in, mp_obj_t sig_in) {
    mp_int_t pid = mp_obj_get_int(pid_in);
    mp_int_t sig = mp_obj_get_int(sig_in);
    int ret;
    MP_OS_CALL(ret, kill, pid, sig);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_os_kill_obj, mp_os_kill);

STATIC mp_obj_t mp_os_times_result(clock_t elapsed, const struct tms *buf) {
    static const qstr mp_os_times_attrs[] = {
        MP_QSTR_user,
        MP_QSTR_system,
        MP_QSTR_children_user,
        MP_QSTR_children_system,
        MP_QSTR_elapsed,
    };
    mp_obj_t items[] = {
        mp_obj_new_int_from_uint(buf->tms_utime),
        mp_obj_new_int_from_uint(buf->tms_stime),
        mp_obj_new_int_from_uint(buf->tms_cutime),
        mp_obj_new_int_from_uint(buf->tms_cstime),
        mp_obj_new_int_from_uint(elapsed),
    };
    return mp_obj_new_attrtuple(mp_os_times_attrs, 5, items);
}

STATIC mp_obj_t mp_os_times(void) {
    struct tms buf;
    clock_t elapsed = times(&buf);
    if (elapsed == -1) {
        mp_raise_OSError(errno);
    }
    return mp_os_times_result(elapsed, &buf);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(mp_os_times_obj, mp_os_times);

// Random numbers
// --------------
STATIC mp_obj_t mp_os_urandom(mp_obj_t size_in) {
    mp_int_t size = mp_obj_get_int(size_in);
    vstr_t buf;
    vstr_init(&buf, size);
    ssize_t ret;
    MP_OS_CALL(ret, getrandom, vstr_str(&buf), size, 0);
    mp_os_check_ret(ret);
    vstr_add_len(&buf, ret);
    return mp_obj_new_bytes_from_vstr(&buf);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_urandom_obj, mp_os_urandom);

// MicroPython extensions
// ----------------------
STATIC mp_obj_t mp_os_dlerror(void) {
    void *error = dlerror();
    if (!error) {
        return mp_const_none;
    }
    size_t error_len = strlen(error);
    mp_obj_t args[] = {
        MP_OBJ_NEW_SMALL_INT(errno),
        mp_obj_new_str_copy(&mp_type_str, error, error_len),
    };
    nlr_raise(mp_obj_exception_make_new(&mp_type_OSError, error_len ? 2 : 1, 0, args));
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_os_dlerror_obj, mp_os_dlerror);

STATIC mp_obj_t mp_os_dlopen(mp_obj_t file_in) {
    const char *file = mp_obj_str_get_str(file_in);
    const void *result = dlopen(file, 0);
    if (!result) {
        mp_raise_OSError(ENOENT);
    }
    return mp_obj_new_int((intptr_t)result);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_os_dlopen_obj, mp_os_dlopen);

STATIC mp_obj_t mp_os_dlsym(mp_obj_t handle_in, mp_obj_t symbol_in) {
    void *handle = (void *)mp_obj_get_int(handle_in);
    const char *symbol = mp_obj_str_get_str(symbol_in);

    const flash_heap_header_t *header = NULL;
    while (dl_iterate(&header) && (header != handle)) {
        ;
    }
    if (header != handle) {
        mp_raise_ValueError(NULL);
    }

    void *value = dlsym(handle, symbol);
    if (!value) {
        mp_raise_type(&mp_type_KeyError);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(mp_os_dlsym_obj, mp_os_dlsym);

STATIC mp_obj_t mp_os_dllist(void) {
    mp_obj_t list = mp_obj_new_list(0, NULL);
    const flash_heap_header_t *header = NULL;
    while (dl_iterate(&header)) {
        Elf32_Addr strtab = 0;
        Elf32_Word soname = 0;
        for (const Elf32_Dyn *dyn = header->entry; dyn->d_tag != DT_NULL; dyn++) {
            switch (dyn->d_tag) {
                case DT_STRTAB:
                    strtab = dyn->d_un.d_ptr;
                    break;
                case DT_SONAME:
                    soname = dyn->d_un.d_val;
                    break;
            }
        }
        if (strtab && soname) {
            void *addr = (void *)(strtab + soname);
            mp_obj_t item = mp_obj_new_str_copy(&mp_type_str, addr, strlen(addr));
            mp_obj_list_append(list, item);
        }
    }
    return list;
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_os_dllist_obj, mp_os_dllist);

STATIC mp_obj_t mp_os_mkfs(mp_obj_t source_in, mp_obj_t type_in) {
    const char *source = mp_obj_str_get_str(source_in);
    const char *type = mp_obj_str_get_str(type_in);
    int ret;
    MP_OS_CALL(ret, mkfs, source, type, NULL);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mp_os_mkfs_obj, mp_os_mkfs);

STATIC mp_obj_t mp_os_mount(size_t n_args, const mp_obj_t *args) {
    const char *source = mp_obj_str_get_str(args[0]);
    const char *target = mp_obj_str_get_str(args[1]);
    const char *type = mp_obj_str_get_str(args[2]);
    mp_int_t flags = n_args > 3 ? mp_obj_get_int(args[3]) : 0;
    int ret;
    MP_OS_CALL(ret, mount, source, target, type, flags, NULL);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_mount_obj, 3, 4, mp_os_mount);

STATIC mp_obj_t mp_os_umount(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret;
    MP_OS_CALL(ret, umount, path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_os_umount_obj, mp_os_umount);


STATIC const mp_rom_map_elem_t os_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),   MP_ROM_QSTR(MP_QSTR_os) },

    // Process Parameters
    { MP_ROM_QSTR(MP_QSTR_environ),    MP_ROM_PTR(&mp_os_environ_obj) },
    { MP_ROM_QSTR(MP_QSTR_getenv),     MP_ROM_PTR(&mp_os_getenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpid),     MP_ROM_PTR(&mp_os_getpid_obj) },
    { MP_ROM_QSTR(MP_QSTR_putenv),     MP_ROM_PTR(&mp_os_putenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_strerror),   MP_ROM_PTR(&mp_os_strerror_obj) },
    #if MICROPY_PY_OS_UNAME
    { MP_ROM_QSTR(MP_QSTR_uname),      MP_ROM_PTR(&mp_os_uname_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_unsetenv),    MP_ROM_PTR(&mp_os_unsetenv_obj) },

    // File Descriptor Operations
    { MP_ROM_QSTR(MP_QSTR_close),      MP_ROM_PTR(&mp_os_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_dup),        MP_ROM_PTR(&mp_os_dup_obj) },
    { MP_ROM_QSTR(MP_QSTR_dup2),       MP_ROM_PTR(&mp_os_dup2_obj) },
    { MP_ROM_QSTR(MP_QSTR_fsync),      MP_ROM_PTR(&mp_os_fsync_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),     MP_ROM_PTR(&mp_os_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_lseek),      MP_ROM_PTR(&mp_os_lseek_obj) },
    { MP_ROM_QSTR(MP_QSTR_open),       MP_ROM_PTR(&mp_os_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),       MP_ROM_PTR(&mp_os_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),      MP_ROM_PTR(&mp_os_write_obj) },

    // Files and Directories
    { MP_ROM_QSTR(MP_QSTR_chdir),      MP_ROM_PTR(&mp_os_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd),     MP_ROM_PTR(&mp_os_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_listdir),    MP_ROM_PTR(&mp_os_listdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir),      MP_ROM_PTR(&mp_os_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove),     MP_ROM_PTR(&mp_os_unlink_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename),     MP_ROM_PTR(&mp_os_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir),      MP_ROM_PTR(&mp_os_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_scandir),    MP_ROM_PTR(&mp_os_scandir_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat),       MP_ROM_PTR(&mp_os_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_statvfs),    MP_ROM_PTR(&mp_os_statvfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_sync),       MP_ROM_PTR(&mp_os_sync_obj) },
    { MP_ROM_QSTR(MP_QSTR_truncate),   MP_ROM_PTR(&mp_os_truncate_obj) },
    { MP_ROM_QSTR(MP_QSTR_unlink),     MP_ROM_PTR(&mp_os_unlink_obj) },

    // Process Management
    { MP_ROM_QSTR(MP_QSTR_abort),      MP_ROM_PTR(&mp_os_abort_obj) },
    { MP_ROM_QSTR(MP_QSTR__exit),      MP_ROM_PTR(&mp_os__exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_kill),       MP_ROM_PTR(&mp_os_kill_obj) },
    #if MICROPY_PY_OS_SYSTEM
    { MP_ROM_QSTR(MP_QSTR_system),     MP_ROM_PTR(&mp_os_system_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_times),      MP_ROM_PTR(&mp_os_times_obj) },

    // Miscellaneous System Information
    { MP_ROM_QSTR(MP_QSTR_curdir),     MP_ROM_QSTR(MP_QSTR__dot_) },
    { MP_ROM_QSTR(MP_QSTR_pardir),     MP_ROM_QSTR(MP_QSTR__dot__dot_) },
    { MP_ROM_QSTR(MP_QSTR_sep),        MP_ROM_QSTR(MP_QSTR__slash_) },

    // Random numbers
    #if MICROPY_PY_OS_URANDOM
    { MP_ROM_QSTR(MP_QSTR_urandom),    MP_ROM_PTR(&mp_os_urandom_obj) },
    #endif

    // The following are MicroPython extensions.
    { MP_ROM_QSTR(MP_QSTR_dlerror),    MP_ROM_PTR(&mp_os_dlerror_obj) },
    { MP_ROM_QSTR(MP_QSTR_dllist),     MP_ROM_PTR(&mp_os_dllist_obj) },
    { MP_ROM_QSTR(MP_QSTR_dlopen),     MP_ROM_PTR(&mp_os_dlopen_obj) },
    { MP_ROM_QSTR(MP_QSTR_dlsym),      MP_ROM_PTR(&mp_os_dlsym_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkfs),       MP_ROM_PTR(&mp_os_mkfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_mount),      MP_ROM_PTR(&mp_os_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount),     MP_ROM_PTR(&mp_os_umount_obj) },


    // Flags for lseek
    { MP_ROM_QSTR(MP_QSTR_SEEK_SET),   MP_ROM_INT(SEEK_SET) },
    { MP_ROM_QSTR(MP_QSTR_SEEK_CUR),   MP_ROM_INT(SEEK_CUR) },
    { MP_ROM_QSTR(MP_QSTR_SEEK_END),   MP_ROM_INT(SEEK_END) },

    // Flags for open
    { MP_ROM_QSTR(MP_QSTR_O_RDONLY),   MP_ROM_INT(O_RDONLY) },
    { MP_ROM_QSTR(MP_QSTR_O_WRONLY),   MP_ROM_INT(O_WRONLY) },
    { MP_ROM_QSTR(MP_QSTR_O_RDWR),     MP_ROM_INT(O_RDWR) },
    { MP_ROM_QSTR(MP_QSTR_O_APPEND),   MP_ROM_INT(O_APPEND) },
    { MP_ROM_QSTR(MP_QSTR_O_CREAT),    MP_ROM_INT(O_CREAT) },
    { MP_ROM_QSTR(MP_QSTR_O_EXCL),     MP_ROM_INT(O_EXCL) },
    { MP_ROM_QSTR(MP_QSTR_O_TRUNC),    MP_ROM_INT(O_TRUNC) },
};
STATIC MP_DEFINE_CONST_DICT(os_module_globals, os_module_globals_table);

const mp_obj_module_t mp_module_os = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&os_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_os, mp_module_os);


typedef struct {
    mp_obj_base_t base;
    int fd;
} mp_io_file_t;

extern const mp_obj_type_t mp_type_io_fileio;
extern const mp_obj_type_t mp_type_io_textio;

STATIC mp_uint_t mp_io_check_ret(int ret, int *errcode) {
    if (ret < 0) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }
    return ret;
}

STATIC mp_io_file_t *mp_io_file_get(mp_obj_t self_in) {
    mp_io_file_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->fd == -1) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed file"));
    }
    return self;
}

mp_import_stat_t mp_import_stat(const char *path) {
    struct stat buf;
    int ret;
    MP_OS_CALL(ret, stat, path, &buf);
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

mp_obj_t mp_builtin_open(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_file, ARG_mode, ARG_buffering, ARG_encoding };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_file, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_NONE} },
        { MP_QSTR_mode, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_QSTR(MP_QSTR_r)} },
        { MP_QSTR_buffering, MP_ARG_INT, {.u_int = -1} },
        { MP_QSTR_encoding, MP_ARG_OBJ, {.u_rom_obj = MP_ROM_NONE} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    mp_io_file_t *self = m_new_obj_with_finaliser(mp_io_file_t);
    self->base.type = &mp_type_io_fileio;
    mp_obj_t fd = args[ARG_file].u_obj;
    if (!mp_obj_is_int(fd)) {
        const char *mode = mp_obj_str_get_str(args[ARG_mode].u_obj);
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
                case 'a':
                    mode_rw = O_WRONLY;
                    mode_x = O_CREAT | O_APPEND;
                    break;
                case '+':
                    mode_rw = O_RDWR;
                    break;
                case 'b':
                    self->base.type = &mp_type_io_fileio;
                    break;
                case 't':
                    self->base.type = &mp_type_io_textio;
                    break;
            }
        }
        mp_obj_t args[] = { fd, mp_obj_new_int(mode_x | mode_rw) };
        fd = mp_os_open(2, args);
    }
    self->fd = mp_obj_get_int(fd);
    return MP_OBJ_FROM_PTR(self);
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);

STATIC mp_obj_t mp_io_file_fileno(mp_obj_t self_in) {
    mp_io_file_t *self = mp_io_file_get(self_in);
    return mp_obj_new_int(self->fd);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_fileno_obj, mp_io_file_fileno);

STATIC mp_obj_t mp_io_file_isatty(mp_obj_t self_in) {
    mp_io_file_t *self = mp_io_file_get(self_in);
    return mp_os_isatty(mp_obj_new_int(self->fd));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mp_io_file_isatty_obj, mp_io_file_isatty);

STATIC mp_obj_t mp_io_file_truncate(size_t n_args, const mp_obj_t *args) {
    mp_io_file_t *self = mp_io_file_get(args[0]);
    mp_obj_t fd = mp_obj_new_int(self->fd);
    mp_obj_t size;
    if ((n_args <= 1) || (args[1] == mp_const_none)) {
        size = mp_os_lseek(fd, MP_OBJ_NEW_SMALL_INT(0), MP_OBJ_NEW_SMALL_INT(SEEK_CUR));
    } else {
        size = args[1];
    }
    return mp_os_truncate(fd, size);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_io_file_truncate_obj, 1, 2, mp_io_file_truncate);

STATIC mp_uint_t mp_io_file_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mp_io_file_t *self = mp_io_file_get(self_in);
    int ret;
    MP_OS_CALL(ret, read, self->fd, buf, size);
    return mp_io_check_ret(ret, errcode);
}

STATIC mp_uint_t mp_io_file_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    mp_io_file_t *self = mp_io_file_get(self_in);
    int ret;
    MP_OS_CALL(ret, write, self->fd, buf, size);
    return mp_io_check_ret(ret, errcode);
}

STATIC mp_uint_t mp_io_file_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    if (request == MP_STREAM_CLOSE) {
        mp_io_file_t *self = MP_OBJ_TO_PTR(self_in);
        if (self->fd >= 0) {
            int ret;
            MP_OS_CALL(ret, close, self->fd);
            self->fd = -1;
        }
        return 0;
    }

    mp_io_file_t *self = mp_io_file_get(self_in);
    switch (request) {
        case MP_STREAM_FLUSH: {
            int ret;
            MP_OS_CALL(ret, fsync, self->fd);
            return mp_io_check_ret(ret, errcode);
        }
        case MP_STREAM_SEEK: {
            struct mp_stream_seek_t *s = (struct mp_stream_seek_t *)arg;
            int ret;
            MP_OS_CALL(ret, lseek, self->fd, s->offset, s->whence);
            if (ret >= 0) {
                s->offset = ret;
            }
            return mp_io_check_ret(ret, errcode);
        }
        case MP_STREAM_GET_FILENO: {
            return self->fd;
        }
        default: {
            *errcode = EINVAL;
            return MP_STREAM_ERROR;
        }
    }
}

STATIC const mp_rom_map_elem_t mp_io_file_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_fileno),     MP_ROM_PTR(&mp_io_file_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),     MP_ROM_PTR(&mp_io_file_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_truncate),   MP_ROM_PTR(&mp_io_file_truncate_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),       MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),   MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline),   MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_readlines),  MP_ROM_PTR(&mp_stream_unbuffered_readlines_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),      MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_seek),       MP_ROM_PTR(&mp_stream_seek_obj) },
    { MP_ROM_QSTR(MP_QSTR_tell),       MP_ROM_PTR(&mp_stream_tell_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),      MP_ROM_PTR(&mp_stream_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),      MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__),    MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__),  MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),   MP_ROM_PTR(&mp_stream___exit___obj) },
};

STATIC MP_DEFINE_CONST_DICT(mp_io_file_locals_dict, mp_io_file_locals_dict_table);

STATIC const mp_stream_p_t mp_io_fileio_stream_p = {
    .read = mp_io_file_read,
    .write = mp_io_file_write,
    .ioctl = mp_io_file_ioctl,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_io_fileio,
    MP_QSTR_FileIO,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    // print, mp_io_file_print,
    protocol, &mp_io_fileio_stream_p,
    locals_dict, &mp_io_file_locals_dict
    );

STATIC const mp_stream_p_t mp_io_textio_stream_p = {
    .read = mp_io_file_read,
    .write = mp_io_file_write,
    .ioctl = mp_io_file_ioctl,
    .is_text = true,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_io_textio,
    MP_QSTR_TextIOWrapper,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    // print, mp_io_file_print,
    protocol, &mp_io_textio_stream_p,
    locals_dict, &mp_io_file_locals_dict
    );

mp_io_file_t mp_sys_stdin_obj = {{&mp_type_io_textio}, STDIN_FILENO};
mp_io_file_t mp_sys_stdout_obj = {{&mp_type_io_textio}, STDOUT_FILENO};
mp_io_file_t mp_sys_stderr_obj = {{&mp_type_io_textio}, STDERR_FILENO};

#endif // MICROPY_PY_OS
