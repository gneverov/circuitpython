// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <string.h>
#include <sys/time.h>

#include "newlib/time.h"

#include "py/mphal.h"
#include "py/objint.h"
#include "py/objstr.h"
#include "py/runtime.h"

#if MICROPY_PY_TIME

#ifdef MICROPY_PY_TIME_INCLUDEFILE
#include MICROPY_PY_TIME_INCLUDEFILE
#endif

static mp_obj_t mp_time_tm_to_obj(const struct tm *tm) {
    static const qstr mp_time_tm_attrs[] = {
        MP_QSTR_tm_year,
        MP_QSTR_tm_mon,
        MP_QSTR_tm_day,
        MP_QSTR_tm_hour,
        MP_QSTR_tm_min,
        MP_QSTR_tm_sec,
        MP_QSTR_tm_wday,
        MP_QSTR_tm_yday,
        MP_QSTR_tm_isdst,
    };
    mp_obj_t items[] = {
        mp_obj_new_int(tm->tm_year + 1900),
        mp_obj_new_int(tm->tm_mon + 1),
        mp_obj_new_int(tm->tm_mday),
        mp_obj_new_int(tm->tm_hour),
        mp_obj_new_int(tm->tm_min),
        mp_obj_new_int(tm->tm_sec),
        mp_obj_new_int(tm->tm_wday),
        mp_obj_new_int(tm->tm_yday),
        mp_obj_new_int(tm->tm_isdst),
    };
    return mp_obj_new_attrtuple(mp_time_tm_attrs, 9, items);
}

static void mp_time_obj_to_tm(mp_obj_t obj, struct tm *tm) {
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(obj, &len, &items);
    if (len != 9) {
        mp_raise_TypeError(NULL);
    }
    tm->tm_year = mp_obj_get_int(items[0] - 1900);
    tm->tm_mon = mp_obj_get_int(items[1] - 1);
    tm->tm_mday = mp_obj_get_int(items[2]);
    tm->tm_hour = mp_obj_get_int(items[3]);
    tm->tm_min = mp_obj_get_int(items[4]);
    tm->tm_sec = mp_obj_get_int(items[5]);
    tm->tm_wday = mp_obj_get_int(items[6]);
    tm->tm_yday = mp_obj_get_int(items[7]);
    tm->tm_isdst = mp_obj_get_int(items[8]);
}

static void mp_time_obj_to_time(mp_obj_t obj, time_t *t) {
    if (mp_obj_is_small_int(obj)) {
        *t = MP_OBJ_SMALL_INT_VALUE(obj);
    } else if (mp_obj_is_exact_type(obj, &mp_type_int)) {
        mp_obj_int_to_bytes_impl(obj, false, sizeof(time_t), (byte *)t);
    } else {
        mp_raise_TypeError(NULL);
    }
}

static mp_obj_t mp_time_asctime(size_t n_args, const mp_obj_t *args) {
    struct tm tm;
    if (n_args == 0) {
        time_t t = time(NULL);
        tm = *localtime(&t);
    } else {
        mp_time_obj_to_tm(args[0], &tm);
    }
    char *str = asctime(&tm);
    return mp_obj_new_str_copy(&mp_type_str, (byte *)str, strlen(str) - 1);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_time_asctime_obj, 0, 1, mp_time_asctime);

static mp_obj_t mp_time_ctime(size_t n_args, const mp_obj_t *args) {
    time_t t;
    if ((n_args == 0) || (args[0] == mp_const_none)) {
        time(&t);
    } else {
        mp_time_obj_to_time(args[0], &t);
    }
    char *str = ctime(&t);
    return mp_obj_new_str_copy(&mp_type_str, (byte *)str, strlen(str) - 1);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_time_ctime_obj, 0, 1, mp_time_ctime);

static mp_obj_t mp_time_gmtime(size_t n_args, const mp_obj_t *args) {
    time_t t;
    if ((n_args == 0) || (args[0] == mp_const_none)) {
        time(&t);
    } else {
        mp_time_obj_to_time(args[0], &t);
    }
    struct tm *tm = gmtime(&t);
    if (!tm) {
        mp_raise_type(&mp_type_OverflowError);
    }
    return mp_time_tm_to_obj(tm);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_time_gmtime_obj, 0, 1, mp_time_gmtime);

static mp_obj_t mp_time_localtime(size_t n_args, const mp_obj_t *args) {
    time_t t;
    if ((n_args == 0) || (args[0] == mp_const_none)) {
        time(&t);
    } else {
        mp_time_obj_to_time(args[0], &t);
    }
    struct tm *tm = localtime(&t);
    if (!tm) {
        mp_raise_type(&mp_type_OverflowError);
    }
    return mp_time_tm_to_obj(tm);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_time_localtime_obj, 0, 1, mp_time_localtime);

static mp_obj_t mp_time_mktime(mp_obj_t obj) {
    struct tm tm;
    mp_time_obj_to_tm(obj, &tm);
    time_t t = mktime(&tm);
    if (t == -1) {
        mp_raise_type(&mp_type_OverflowError);
    }
    return mp_obj_new_int_from_ll(t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_time_mktime_obj, mp_time_mktime);

static mp_obj_t mp_time_monotonic(void) {
    mp_uint_t ticks = mp_hal_ticks_us();
    return mp_obj_new_float(ticks * 1e-6);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_time_monotonic_obj, mp_time_monotonic);

static mp_obj_t mp_time_monotonic_ns(void) {
    mp_uint_t ticks = mp_hal_ticks_us();
    return mp_obj_new_int(ticks * 1000);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_time_monotonic_ns_obj, mp_time_monotonic_ns);

static mp_obj_t mp_time_sleep(mp_obj_t secs_in) {
    mp_float_t secs = mp_obj_get_float(secs_in);
    struct timespec t = { .tv_sec = secs, .tv_nsec = 1e9 * secs};
    for (;;) {
        MP_THREAD_GIL_EXIT();
        int ret = nanosleep(&t, &t);
        MP_THREAD_GIL_ENTER();
        if ((ret >= 0) || (errno != EINTR)) {
            break;
        } else {
            mp_handle_pending(true);
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_time_sleep_obj, mp_time_sleep);

static mp_obj_t mp_time_strftime(size_t n_args, const mp_obj_t *args) {
    const char *format = mp_obj_str_get_str(args[0]);
    struct tm tm;
    if (n_args == 1) {
        time_t t = time(NULL);
        tm = *localtime(&t);
    } else {
        mp_time_obj_to_tm(args[1], &tm);
    }
    vstr_t vstr;
    vstr_init_len(&vstr, 256);
    size_t len = strftime(vstr.buf, 256, format, &tm);
    if (!len) {
        mp_raise_ValueError(NULL);
    }
    vstr_add_len(&vstr, len);
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_time_strftime_obj, 1, 2, mp_time_strftime);

static mp_obj_t mp_time_strptime(size_t n_args, const mp_obj_t *args) {
    const char *str = mp_obj_str_get_str(args[0]);
    const char *format = "%a %b %d %H:%M:%S %Y";
    if (n_args != 1) {
        format = mp_obj_str_get_str(args[1]);
    }
    extern char *strptime(const char *__restrict, const char *__restrict, struct tm *__restrict);
    struct tm tm;
    if (!strptime(str, format, &tm)) {
        mp_raise_ValueError(NULL);
    }
    return mp_time_tm_to_obj(&tm);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_time_strptime_obj, 1, 2, mp_time_strptime);

static mp_obj_t mp_time_time(void) {
    time_t t = time(NULL);
    return mp_obj_new_float(t);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_time_time_obj, mp_time_time);

static mp_obj_t mp_time_time_ns(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return mp_obj_new_int_from_ll((tv.tv_sec * 1000000 + tv.tv_usec) * 1000);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_time_time_ns_obj, mp_time_time_ns);

static mp_obj_t mp_time_tzset(void) {
    tzset();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_time_tzset_obj, mp_time_tzset);

static mp_obj_t mp_time_getattr(mp_obj_t attr) {
    switch (MP_OBJ_QSTR_VALUE(attr)) {
        case MP_QSTR_daylight:
            return MP_OBJ_NEW_SMALL_INT(_daylight);
        case MP_QSTR_timezone:
            return mp_obj_new_int(_timezone);
        case MP_QSTR_tzname: {
            mp_obj_t items[] = {
                mp_obj_new_str_copy(&mp_type_str, (byte *)_tzname[0], strlen(_tzname[0])),
                mp_obj_new_str_copy(&mp_type_str, (byte *)_tzname[1], strlen(_tzname[1])),
            };
            return mp_obj_new_tuple(2, items);
        }
        default:
            return MP_OBJ_NULL;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_time_getattr_obj, mp_time_getattr);

static const mp_rom_map_elem_t mp_module_time_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_time) },
    { MP_ROM_QSTR(MP_QSTR___getattr__),     MP_ROM_PTR(&mp_time_getattr_obj) },

    { MP_ROM_QSTR(MP_QSTR_asctime),         MP_ROM_PTR(&mp_time_asctime_obj) },
    { MP_ROM_QSTR(MP_QSTR_ctime),           MP_ROM_PTR(&mp_time_ctime_obj) },
    { MP_ROM_QSTR(MP_QSTR_gmtime),          MP_ROM_PTR(&mp_time_gmtime_obj) },
    { MP_ROM_QSTR(MP_QSTR_localtime),       MP_ROM_PTR(&mp_time_localtime_obj) },
    { MP_ROM_QSTR(MP_QSTR_mktime),          MP_ROM_PTR(&mp_time_mktime_obj) },
    { MP_ROM_QSTR(MP_QSTR_monotonic),       MP_ROM_PTR(&mp_time_monotonic_obj) },
    { MP_ROM_QSTR(MP_QSTR_monotonic_ns),    MP_ROM_PTR(&mp_time_monotonic_ns_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep),           MP_ROM_PTR(&mp_time_sleep_obj) },
    { MP_ROM_QSTR(MP_QSTR_strftime),        MP_ROM_PTR(&mp_time_strftime_obj) },
    { MP_ROM_QSTR(MP_QSTR_strptime),        MP_ROM_PTR(&mp_time_strptime_obj) },
    { MP_ROM_QSTR(MP_QSTR_time),            MP_ROM_PTR(&mp_time_time_obj) },
    { MP_ROM_QSTR(MP_QSTR_time_ns),         MP_ROM_PTR(&mp_time_time_ns_obj) },
    { MP_ROM_QSTR(MP_QSTR_tzset),           MP_ROM_PTR(&mp_time_tzset_obj) },

    #ifdef MICROPY_PY_TIME_EXTRA_GLOBALS
    MICROPY_PY_TIME_EXTRA_GLOBALS
    #endif
};
static MP_DEFINE_CONST_DICT(mp_module_time_globals, mp_module_time_globals_table);

const mp_obj_module_t mp_module_time = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_time_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_time, mp_module_time);

#endif // MICROPY_PY_TIME
