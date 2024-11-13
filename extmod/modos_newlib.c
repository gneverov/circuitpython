// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <unistd.h>
#include "newlib/dlfcn.h"
#include "newlib/event.h"
#include "newlib/ioctl.h"
#include "newlib/mount.h"
#include "newlib/newlib.h"

#include "extmod/modos_newlib.h"
#include "py/objstr.h"
#include "py/runtime.h"


__attribute__((visibility("hidden")))
mp_obj_t mp_os_check_ret(int ret) {
    if (ret >= 0) {
        return mp_obj_new_int(ret);
    } else {
        mp_raise_OSError(errno);
    }
}

__attribute__((visibility("hidden")))
bool mp_os_nonblocking_ret(int ret) {
    return (ret < 0) && (errno == EAGAIN);
}

__attribute__((visibility("hidden")))
int mp_os_get_fd(mp_obj_t obj_in) {
    if (!mp_obj_is_int(obj_in)) {
        mp_obj_t args[2];
        mp_load_method(obj_in, MP_QSTR_fileno, args);
        obj_in = mp_call_method_n_kw(0, 0, args);
    }
    return mp_obj_get_int(obj_in);
}

bool mp_os_event_wait(int fd, uint events) {
    int ret;
    MP_OS_CALL(ret, event_wait, fd, events);
    if (mp_os_nonblocking_ret(ret)) {
        return false;
    }
    mp_os_check_ret(ret);
    return true;
}

static const MP_DEFINE_STR_OBJ(mp_os_name_obj, "posix");


// Process Parameters
// ------------------
static mp_obj_t mp_os_environ(void) {
    mp_obj_t dict = mp_obj_new_dict(0);
    extern char **environ;
    char **env = environ;
    while (*env) {
        char *equal = strchr(*env, '=');
        if (equal) {
            mp_obj_t key = mp_obj_new_str(*env, equal - *env);
            mp_obj_t value = mp_obj_new_str_copy(&mp_type_str, (byte *)equal + 1, strlen(equal) - 1);
            mp_obj_dict_store(dict, key, value);
        }
        env++;
    }
    return dict;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_environ_obj, mp_os_environ);

static mp_obj_t mp_os_getenv(mp_obj_t key_in) {
    const char *key = mp_obj_str_get_str(key_in);
    char *value = getenv(key);
    if (!value) {
        return mp_const_none;
    }
    size_t len = strlen(value);
    return mp_obj_new_str_copy(&mp_type_str, (byte *)value, len);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_getenv_obj, mp_os_getenv);

static mp_obj_t mp_os_getpid(void) {
    int pid = getpid();
    return mp_obj_new_int(pid);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_getpid_obj, mp_os_getpid);

static mp_obj_t mp_os_putenv(mp_obj_t key_in, mp_obj_t value_in) {
    const char *key = mp_obj_str_get_str(key_in);
    const char *value = mp_obj_str_get_str(value_in);
    int ret = setenv(key, value, 1);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_putenv_obj, mp_os_putenv);

static mp_obj_t mp_os_strerror(mp_obj_t code_in) {
    mp_int_t code = mp_obj_get_int(code_in);
    char *s = strerror(code);
    if (!s) {
        mp_raise_ValueError(NULL);
    }
    return mp_obj_new_str_copy(&mp_type_str, (byte *)s, strlen(s));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_strerror_obj, mp_os_strerror);

static mp_obj_t mp_os_uname(void) {
    // Allocate potentially large struct on heap
    struct utsname *name = m_malloc(sizeof(struct utsname));
    int ret = uname(name);
    mp_os_check_ret(ret);

    static const qstr mp_os_uname_attrs[] = {
        MP_QSTR_sysname,
        MP_QSTR_nodename,
        MP_QSTR_release,
        MP_QSTR_version,
        MP_QSTR_machine,
    };
    mp_obj_t items[] = {
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->sysname, strnlen(name->sysname, UTSNAME_LENGTH)),
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->nodename, strnlen(name->nodename, UTSNAME_LENGTH)),
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->release, strnlen(name->release, UTSNAME_LENGTH)),
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->version, strnlen(name->version, UTSNAME_LENGTH)),
        mp_obj_new_str_copy(&mp_type_str, (byte *)name->machine, strnlen(name->machine, UTSNAME_LENGTH)),
    };
    return mp_obj_new_attrtuple(mp_os_uname_attrs, 5, items);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_uname_obj, mp_os_uname);

static mp_obj_t mp_os_unsetenv(mp_obj_t key_in) {
    const char *key = mp_obj_str_get_str(key_in);
    int ret = unsetenv(key);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_unsetenv_obj, mp_os_unsetenv);


// File Descriptor Operations
// --------------------------
static mp_obj_t mp_os_close(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret = close(fd);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_close_obj, mp_os_close);

static mp_obj_t mp_os_dup(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret = dup(fd);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_dup_obj, mp_os_dup);

static mp_obj_t mp_os_dup2(mp_obj_t fd1_in, mp_obj_t fd2_in) {
    mp_int_t fd1 = mp_obj_get_int(fd1_in);
    mp_int_t fd2 = mp_obj_get_int(fd2_in);
    int ret = dup2(fd1, fd2);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_dup2_obj, mp_os_dup2);

static mp_obj_t mp_os_fsync(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret = fsync(fd);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_fsync_obj, mp_os_fsync);

__attribute__((visibility("hidden")))
mp_obj_t mp_os_isatty(mp_obj_t fd_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    int ret = isatty(fd);
    mp_os_check_ret(ret);
    return mp_obj_new_bool(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_isatty_obj, mp_os_isatty);

__attribute__((visibility("hidden")))
mp_obj_t mp_os_lseek(mp_obj_t fd_in, mp_obj_t pos_in, mp_obj_t whence_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    mp_int_t pos = mp_obj_get_int(pos_in);
    mp_int_t whence = mp_obj_get_int(whence_in);
    int ret = lseek(fd, pos, whence);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_3(mp_os_lseek_obj, mp_os_lseek);

static mp_obj_t mp_os_open(size_t n_args, const mp_obj_t *args) {
    const char *path = mp_obj_str_get_str(args[0]);
    mp_int_t flags = mp_obj_get_int(args[1]);
    mp_int_t mode = n_args > 2 ? mp_obj_get_int(args[2]) : 0777;
    int ret;
    MP_OS_CALL(ret, open, path, flags, mode);
    return mp_os_check_ret(ret);
}
__attribute__((visibility("hidden")))
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_open_obj, 2, 3, mp_os_open);

static mp_obj_t mp_os_pipe(void) {
    int fds[2];
    int ret = pipe(fds);
    mp_os_check_ret(ret);
    mp_obj_t items[] = {
        MP_OBJ_NEW_SMALL_INT(fds[0]),
        MP_OBJ_NEW_SMALL_INT(fds[1]),
    };
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_pipe_obj, mp_os_pipe);

__attribute__((visibility("hidden")))
int mp_os_read_vstr(int fd, vstr_t *vstr, size_t size) {
    vstr_hint_size(vstr, size);
    int ret;
    MP_OS_CALL(ret, read, fd, vstr_str(vstr) + vstr_len(vstr), size);
    if (ret > 0) {
        vstr_add_len(vstr, ret);
    }
    return ret;
}

__attribute__((visibility("hidden")))
int mp_os_write_str(int fd, const char *str, size_t len) {
    int ret;
    MP_OS_CALL(ret, write, fd, str, len);
    return ret;
}

static mp_obj_t mp_os_read(mp_obj_t fd_in, mp_obj_t n_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    mp_int_t n = mp_obj_get_int(n_in);
    vstr_t buf;
    vstr_init(&buf, n);
    int ret = mp_os_read_vstr(fd, &buf, n);
    mp_os_check_ret(ret);
    return mp_obj_new_bytes_from_vstr(&buf);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_read_obj, mp_os_read);

static mp_obj_t mp_os_sendfile(size_t n_args, const mp_obj_t *args) {
    int out_fd = mp_os_get_fd(args[0]);
    int in_fd = mp_os_get_fd(args[1]);
    int count = mp_obj_get_int(args[3]);

    if (args[2] != mp_const_none) {
        mp_os_lseek(MP_OBJ_NEW_SMALL_INT(in_fd), args[2], MP_OBJ_NEW_SMALL_INT(SEEK_SET));
    }

    vstr_t buf;
    vstr_init(&buf, MP_OS_DEFAULT_BUFFER_SIZE);
    int progress = 0;
    while (progress < count) {
        int br = mp_os_read_vstr(in_fd, &buf, MIN(count - progress, MP_OS_DEFAULT_BUFFER_SIZE));
        if (br == 0) {
            break;
        }
        mp_os_check_ret(br);
        int bw = 0;
        while (bw < br) {
            int ret = mp_os_write_str(out_fd, vstr_str(&buf) + bw, br - bw);
            mp_os_check_ret(ret);
            bw += ret;
        }
        progress += br;
        vstr_clear(&buf);
    }
    return mp_obj_new_int(progress);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_sendfile_obj, 4, 4, mp_os_sendfile);

static mp_obj_t mp_os_write(mp_obj_t fd_in, mp_obj_t str_in) {
    mp_int_t fd = mp_obj_get_int(fd_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(str_in, &bufinfo, MP_BUFFER_READ);
    int ret = mp_os_write_str(fd, bufinfo.buf, bufinfo.len);
    return mp_os_check_ret(ret);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_write_obj, mp_os_write);


// Files and Directories
// ---------------------
static mp_obj_t mp_os_chdir(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret = chdir(path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_chdir_obj, mp_os_chdir);

static mp_obj_t mp_os_getcwd(void) {
    vstr_t buf;
    vstr_init(&buf, 256);
    const char *cwd = getcwd(vstr_str(&buf), 256);
    vstr_add_len(&buf, strnlen(cwd, 256));
    return mp_obj_new_str_from_vstr(&buf);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_getcwd_obj, mp_os_getcwd);

static mp_obj_t mp_os_scandir(size_t n_args, const mp_obj_t *args);

static mp_obj_t mp_os_listdir(size_t n_args, const mp_obj_t *args) {
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
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_listdir_obj, 0, 1, mp_os_listdir);

static mp_obj_t mp_os_mkdir(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret = mkdir(path, 0777);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_mkdir_obj, mp_os_mkdir);

static mp_obj_t mp_os_rename(mp_obj_t src_in, mp_obj_t dst_in) {
    const char *src = mp_obj_str_get_str(src_in);
    const char *dst = mp_obj_str_get_str(dst_in);
    int ret = rename(src, dst);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_rename_obj, mp_os_rename);

static mp_obj_t mp_os_rmdir(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret = rmdir(path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_rmdir_obj, mp_os_rmdir);

typedef struct {
    mp_obj_base_t base;
    mp_fun_1_t iternext;
    mp_fun_1_t finaliser;
    const mp_obj_type_t *type;
    DIR *dirp;
} mp_os_scandir_iter_t;

static mp_obj_t mp_os_scandir_iter_del(mp_obj_t self_in) {
    mp_os_scandir_iter_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->dirp) {
        closedir(self->dirp);
        self->dirp = NULL;
    }
    return mp_const_none;
}

static mp_obj_t mp_os_scandir_iter_next(mp_obj_t self_in) {
    mp_os_scandir_iter_t *self = MP_OBJ_TO_PTR(self_in);
    if (!self->dirp) {
        return MP_OBJ_STOP_ITERATION;
    }

    errno = 0;
    struct dirent *dp = readdir(self->dirp);
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

static mp_obj_t mp_os_scandir(size_t n_args, const mp_obj_t *args) {
    const char *path = ".";
    const mp_obj_type_t *type = &mp_type_str;
    if (n_args > 0) {
        path = mp_obj_str_get_str(args[0]);
        type = mp_obj_get_type(args[0]);
    }
    mp_os_scandir_iter_t *iter = mp_obj_malloc_with_finaliser(mp_os_scandir_iter_t, &mp_type_polymorph_iter_with_finaliser);
    iter->iternext = mp_os_scandir_iter_next;
    iter->finaliser = mp_os_scandir_iter_del;
    iter->type = type;
    iter->dirp = opendir(path);
    if (!iter->dirp) {
        mp_raise_OSError(errno);
    }
    return MP_OBJ_FROM_PTR(iter);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_scandir_obj, 0, 1, mp_os_scandir);

static mp_obj_t mp_os_stat_result(const struct stat *sb) {
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

static mp_obj_t mp_os_stat(mp_obj_t path_in) {
    int ret;
    struct stat sb;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        ret = fstat(fd, &sb);
    } else {
        const char *path = mp_obj_str_get_str(path_in);
        ret = stat(path, &sb);
    }
    mp_os_check_ret(ret);
    return mp_os_stat_result(&sb);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_stat_obj, mp_os_stat);

static mp_obj_t mp_os_statvfs_result(const struct statvfs *sb) {
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

static mp_obj_t mp_os_statvfs(mp_obj_t path_in) {
    int ret;
    struct statvfs sb;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        ret = fstatvfs(fd, &sb);
    } else {
        const char *path = mp_obj_str_get_str(path_in);
        ret = statvfs(path, &sb);
    }
    mp_os_check_ret(ret);
    return mp_os_statvfs_result(&sb);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_statvfs_obj, mp_os_statvfs);

static mp_obj_t mp_os_sync(void) {
    MP_THREAD_GIL_EXIT();
    sync();
    MP_THREAD_GIL_ENTER();
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_os_sync_obj, mp_os_sync);

__attribute__((visibility("hidden")))
mp_obj_t mp_os_truncate(mp_obj_t path_in, mp_obj_t length_in) {
    int ret;
    if (mp_obj_is_int(path_in)) {
        mp_int_t fd = mp_obj_get_int(path_in);
        mp_int_t length = mp_obj_get_int(length_in);
        ret = ftruncate(fd, length);
    } else {
        const char *path = mp_obj_str_get_str(path_in);
        mp_int_t length = mp_obj_get_int(length_in);
        ret = truncate(path, length);
    }
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_truncate_obj, mp_os_truncate);

static mp_obj_t mp_os_unlink(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret = unlink(path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_unlink_obj, mp_os_unlink);


// Process Management
// ------------------
static mp_obj_t mp_os_abort(void) {
    abort();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_abort_obj, mp_os_abort);

static mp_obj_t mp_os__exit(mp_obj_t n_in) {
    mp_int_t n = mp_obj_get_int(n_in);
    _exit(n);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os__exit_obj, mp_os__exit);

static mp_obj_t mp_os_kill(mp_obj_t pid_in, mp_obj_t sig_in) {
    mp_int_t pid = mp_obj_get_int(pid_in);
    mp_int_t sig = mp_obj_get_int(sig_in);
    MP_THREAD_GIL_EXIT();
    int ret = kill(pid, sig);
    MP_THREAD_GIL_ENTER();
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_kill_obj, mp_os_kill);

static mp_obj_t mp_os_times_result(clock_t elapsed, const struct tms *buf) {
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

static mp_obj_t mp_os_times(void) {
    struct tms buf;
    clock_t elapsed = times(&buf);
    if (elapsed == -1) {
        mp_raise_OSError(errno);
    }
    return mp_os_times_result(elapsed, &buf);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_os_times_obj, mp_os_times);


// Random numbers
// --------------
static mp_obj_t mp_os_urandom(mp_obj_t size_in) {
    mp_int_t size = mp_obj_get_int(size_in);
    vstr_t buf;
    vstr_init(&buf, size);
    MP_THREAD_GIL_EXIT();
    ssize_t ret = getrandom(vstr_str(&buf), size, 0);
    MP_THREAD_GIL_ENTER();
    mp_os_check_ret(ret);
    vstr_add_len(&buf, ret);
    return mp_obj_new_bytes_from_vstr(&buf);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_urandom_obj, mp_os_urandom);


// MicroPython extensions
// ----------------------
static mp_obj_t mp_os_dlerror(void) {
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

static mp_obj_t mp_os_dlflash(mp_obj_t file_in) {
    const char *file = mp_obj_str_get_str(file_in);
    void *result = dl_flash(file);
    if (!result) {
        mp_raise_OSError(errno);
    }
    return mp_obj_new_int((intptr_t)result);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_os_dlflash_obj, mp_os_dlflash);

static mp_obj_t mp_os_dlopen(mp_obj_t file_in) {
    const char *file = mp_obj_str_get_str(file_in);
    void *result = dlopen(file, 0);
    if (!result) {
        mp_raise_OSError(errno);
    }
    return mp_obj_new_int((intptr_t)result);
}
MP_DEFINE_CONST_FUN_OBJ_1(mp_os_dlopen_obj, mp_os_dlopen);

static mp_obj_t mp_os_dlsym(mp_obj_t handle_in, mp_obj_t symbol_in) {
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

static mp_obj_t mp_os_dllist(void) {
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

#ifndef NDEBUG
static mp_obj_t mp_os_dldebug(void) {
    puts("Module               Flash base RAM base");
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
        const char *name = (strtab && soname) ? (char *)(strtab + soname) : NULL;
        uintptr_t flash_base = (((uintptr_t)header) + sizeof(flash_heap_header_t) + 7) & ~7;
        uintptr_t ram_base = (((uintptr_t)header->ram_base) + 7) & ~7;
        printf("%-20s 0x%08x 0x%08x\n", name, flash_base, ram_base);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(mp_os_dldebug_obj, mp_os_dldebug);
#endif

static mp_obj_t mp_os_mkfs(mp_obj_t source_in, mp_obj_t type_in) {
    const char *source = mp_obj_str_get_str(source_in);
    const char *type = mp_obj_str_get_str(type_in);
    int ret;
    MP_OS_CALL(ret, mkfs, source, type, NULL);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_os_mkfs_obj, mp_os_mkfs);

static mp_obj_t mp_os_mount(size_t n_args, const mp_obj_t *args) {
    const char *source = mp_obj_str_get_str(args[0]);
    const char *target = mp_obj_str_get_str(args[1]);
    const char *type = mp_obj_str_get_str(args[2]);
    mp_int_t flags = n_args > 3 ? mp_obj_get_int(args[3]) : 0;
    int ret;
    MP_OS_CALL(ret, mount, source, target, type, flags, NULL);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_os_mount_obj, 3, 4, mp_os_mount);

static mp_obj_t mp_os_umount(mp_obj_t path_in) {
    const char *path = mp_obj_str_get_str(path_in);
    int ret;
    MP_OS_CALL(ret, umount, path);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_os_umount_obj, mp_os_umount);


static const mp_rom_map_elem_t os_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_os) },
    { MP_ROM_QSTR(MP_QSTR_error),       MP_ROM_PTR(&mp_type_OSError) },
    { MP_ROM_QSTR(MP_QSTR_name),        MP_ROM_PTR(&mp_os_name_obj) },

    // Process Parameters
    { MP_ROM_QSTR(MP_QSTR_environ),     MP_ROM_PTR(&mp_os_environ_obj) },
    { MP_ROM_QSTR(MP_QSTR_getenv),      MP_ROM_PTR(&mp_os_getenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpid),      MP_ROM_PTR(&mp_os_getpid_obj) },
    { MP_ROM_QSTR(MP_QSTR_putenv),      MP_ROM_PTR(&mp_os_putenv_obj) },
    { MP_ROM_QSTR(MP_QSTR_strerror),    MP_ROM_PTR(&mp_os_strerror_obj) },
    { MP_ROM_QSTR(MP_QSTR_uname),       MP_ROM_PTR(&mp_os_uname_obj) },
    { MP_ROM_QSTR(MP_QSTR_unsetenv),    MP_ROM_PTR(&mp_os_unsetenv_obj) },

    // File Descriptor Operations
    { MP_ROM_QSTR(MP_QSTR_close),       MP_ROM_PTR(&mp_os_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_dup),         MP_ROM_PTR(&mp_os_dup_obj) },
    { MP_ROM_QSTR(MP_QSTR_dup2),        MP_ROM_PTR(&mp_os_dup2_obj) },
    { MP_ROM_QSTR(MP_QSTR_fsync),       MP_ROM_PTR(&mp_os_fsync_obj) },
    { MP_ROM_QSTR(MP_QSTR_isatty),      MP_ROM_PTR(&mp_os_isatty_obj) },
    { MP_ROM_QSTR(MP_QSTR_lseek),       MP_ROM_PTR(&mp_os_lseek_obj) },
    { MP_ROM_QSTR(MP_QSTR_open),        MP_ROM_PTR(&mp_os_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_pipe),        MP_ROM_PTR(&mp_os_pipe_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),        MP_ROM_PTR(&mp_os_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendfile),    MP_ROM_PTR(&mp_os_sendfile_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),       MP_ROM_PTR(&mp_os_write_obj) },

    // Files and Directories
    { MP_ROM_QSTR(MP_QSTR_chdir),       MP_ROM_PTR(&mp_os_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd),      MP_ROM_PTR(&mp_os_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_listdir),     MP_ROM_PTR(&mp_os_listdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir),       MP_ROM_PTR(&mp_os_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove),      MP_ROM_PTR(&mp_os_unlink_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename),      MP_ROM_PTR(&mp_os_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir),       MP_ROM_PTR(&mp_os_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_scandir),     MP_ROM_PTR(&mp_os_scandir_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat),        MP_ROM_PTR(&mp_os_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_statvfs),     MP_ROM_PTR(&mp_os_statvfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_sync),        MP_ROM_PTR(&mp_os_sync_obj) },
    { MP_ROM_QSTR(MP_QSTR_truncate),    MP_ROM_PTR(&mp_os_truncate_obj) },
    { MP_ROM_QSTR(MP_QSTR_unlink),      MP_ROM_PTR(&mp_os_unlink_obj) },

    // Process Management
    { MP_ROM_QSTR(MP_QSTR_abort),       MP_ROM_PTR(&mp_os_abort_obj) },
    { MP_ROM_QSTR(MP_QSTR__exit),       MP_ROM_PTR(&mp_os__exit_obj) },
    { MP_ROM_QSTR(MP_QSTR_kill),        MP_ROM_PTR(&mp_os_kill_obj) },
    #if MICROPY_PY_OS_SYSTEM
    { MP_ROM_QSTR(MP_QSTR_system),      MP_ROM_PTR(&mp_os_system_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_times),       MP_ROM_PTR(&mp_os_times_obj) },

    // Miscellaneous System Information
    { MP_ROM_QSTR(MP_QSTR_curdir),      MP_ROM_QSTR(MP_QSTR__dot_) },
    { MP_ROM_QSTR(MP_QSTR_pardir),      MP_ROM_QSTR(MP_QSTR__dot__dot_) },
    { MP_ROM_QSTR(MP_QSTR_sep),         MP_ROM_QSTR(MP_QSTR__slash_) },
    { MP_ROM_QSTR(MP_QSTR_altsep),      MP_ROM_NONE },
    { MP_ROM_QSTR(MP_QSTR_extsep),      MP_ROM_QSTR(MP_QSTR__dot_) },
    { MP_ROM_QSTR(MP_QSTR_pathsep),     MP_ROM_QSTR(MP_QSTR__colon_) },
    { MP_ROM_QSTR(MP_QSTR_linesep),     MP_ROM_QSTR(MP_QSTR__0x0a_) },
    // { MP_ROM_QSTR(MP_QSTR_devnull),     MP_ROM_QSTR(MP_QSTR_/dev/null) },

    // Random numbers
    { MP_ROM_QSTR(MP_QSTR_urandom),     MP_ROM_PTR(&mp_os_urandom_obj) },

    // The following are MicroPython extensions.
    { MP_ROM_QSTR(MP_QSTR_dlerror),     MP_ROM_PTR(&mp_os_dlerror_obj) },
    { MP_ROM_QSTR(MP_QSTR_dlflash),     MP_ROM_PTR(&mp_os_dlflash_obj) },
    { MP_ROM_QSTR(MP_QSTR_dllist),      MP_ROM_PTR(&mp_os_dllist_obj) },
    { MP_ROM_QSTR(MP_QSTR_dlopen),      MP_ROM_PTR(&mp_os_dlopen_obj) },
    { MP_ROM_QSTR(MP_QSTR_dlsym),       MP_ROM_PTR(&mp_os_dlsym_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkfs),        MP_ROM_PTR(&mp_os_mkfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_mount),       MP_ROM_PTR(&mp_os_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount),      MP_ROM_PTR(&mp_os_umount_obj) },
    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_dldebug),     MP_ROM_PTR(&mp_os_dldebug_obj) },
    #endif


    // Flags for lseek
    { MP_ROM_QSTR(MP_QSTR_SEEK_SET),    MP_ROM_INT(SEEK_SET) },
    { MP_ROM_QSTR(MP_QSTR_SEEK_CUR),    MP_ROM_INT(SEEK_CUR) },
    { MP_ROM_QSTR(MP_QSTR_SEEK_END),    MP_ROM_INT(SEEK_END) },

    // Flags for open
    { MP_ROM_QSTR(MP_QSTR_O_RDONLY),    MP_ROM_INT(O_RDONLY) },
    { MP_ROM_QSTR(MP_QSTR_O_WRONLY),    MP_ROM_INT(O_WRONLY) },
    { MP_ROM_QSTR(MP_QSTR_O_RDWR),      MP_ROM_INT(O_RDWR) },
    { MP_ROM_QSTR(MP_QSTR_O_APPEND),    MP_ROM_INT(O_APPEND) },
    { MP_ROM_QSTR(MP_QSTR_O_CREAT),     MP_ROM_INT(O_CREAT) },
    { MP_ROM_QSTR(MP_QSTR_O_EXCL),      MP_ROM_INT(O_EXCL) },
    { MP_ROM_QSTR(MP_QSTR_O_TRUNC),     MP_ROM_INT(O_TRUNC) },
    { MP_ROM_QSTR(MP_QSTR_O_SYNC),      MP_ROM_INT(O_SYNC) },
    { MP_ROM_QSTR(MP_QSTR_O_NONBLOCK),  MP_ROM_INT(O_NONBLOCK) },
    { MP_ROM_QSTR(MP_QSTR_O_NOCTTY),    MP_ROM_INT(O_NOCTTY) },

    // Flags for mount
    { MP_ROM_QSTR(MP_QSTR_MS_RDONLY),   MP_ROM_INT(MS_RDONLY) },
    { MP_ROM_QSTR(MP_QSTR_MS_REMOUNT),  MP_ROM_INT(MS_REMOUNT) },
};
static MP_DEFINE_CONST_DICT(os_module_globals, os_module_globals_table);

const mp_obj_module_t mp_module_os = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&os_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_os, mp_module_os);
