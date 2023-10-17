// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdarg.h>

#include "py/parseargs.h"
#include "py/runtime.h"


STATIC const char *parse_arg(const mp_obj_t arg, const char *format, qstr name, va_list *vals) {
    if (*format == 's') {
        format++;
        if (*format == '*') {
            format++;
            mp_buffer_info_t *val = va_arg(*vals, mp_buffer_info_t *);

            if (arg != MP_OBJ_NULL) {
                mp_buffer_info_t bufinfo;
                mp_get_buffer_raise(arg, &bufinfo, MP_BUFFER_READ);
                if (val) {
                    *val = bufinfo;
                }
            }
        } else {
            const char **val = va_arg(*vals, const char **);

            if (arg != MP_OBJ_NULL) {
                const char *s = mp_obj_str_get_str(arg);
                if (val) {
                    *val = s;
                }
            }
        }
    } else if (*format == 'i') {
        format++;
        mp_int_t *val = va_arg(*vals, mp_int_t *);

        if (arg != MP_OBJ_NULL) {
            mp_int_t i = mp_obj_get_int(arg);
            if (val) {
                *val = i;
            }
        }
    } else if (*format == 'p') {
        format++;
        mp_int_t *val = va_arg(*vals, mp_int_t *);

        if (arg != MP_OBJ_NULL) {
            mp_int_t i = mp_obj_is_true(arg);
            if (val) {
                *val = i;
            }
        }
    } else if (*format == 'O') {
        format++;
        if (*format == '!') {
            format++;
            const mp_obj_type_t *expected_type = va_arg(*vals, const mp_obj_type_t *);
            mp_obj_t *val = va_arg(*vals, mp_obj_t *);

            if (arg != MP_OBJ_NULL) {
                const mp_obj_type_t *actual_type = mp_obj_get_type(arg);
                if (!mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(actual_type), MP_OBJ_FROM_PTR(expected_type))) {
                    mp_raise_msg_varg(&mp_type_TypeError, "%q: must be %q, not %q", name, actual_type->name, expected_type->name);
                }
                if (val) {
                    *val = arg;
                }
            }
        } else if (*format == '&') {
            format++;
            void *(*converter)(mp_obj_t) = va_arg(*vals, void *(*)(mp_obj_t));
            void **val = va_arg(*vals, void **);

            if (arg != MP_OBJ_NULL) {
                if (val) {
                    *val = converter(arg);
                }
            }
        } else {
            mp_obj_t *val = va_arg(*vals, mp_obj_t *);
            if (val && arg != MP_OBJ_NULL) {
                *val = arg;
            }
        }
    }
    return format;
}

void vparse_args_and_kw(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args, const char *format, const qstr keywords[], va_list vals) {
    bool required = true;
    bool pos_allowed = true;
    size_t pos = 0;
    uint32_t used_kws = 0;

    while (*format) {
        qstr name = *keywords;
        if (*format == '|') {
            format++;
            required = false;
            continue;
        }
        if (*format == '$') {
            format++;
            required = false;
            pos_allowed = false;
            continue;
        }

        bool found = false;
        if (pos_allowed && pos < n_args) {
            format = parse_arg(args[pos++], format, name, &vals);
            found = true;
        }

        bool kw_allowed = name > MP_QSTR_;
        if (kw_allowed) {
            mp_map_elem_t *elem = mp_map_lookup(kw_args, MP_OBJ_NEW_QSTR(name), MP_MAP_LOOKUP);
            if (elem) {
                format = parse_arg(elem->value, format, name, &vals);
                used_kws |= 1 << (elem - kw_args->table);
                found = true;
            }
        }

        if (!found) {
            if (required) {
                mp_raise_msg_varg(&mp_type_TypeError, "%q: missing", name);
            } else {
                format = parse_arg(MP_OBJ_NULL, format, name, &vals);
            }
        }
        keywords += (name > 0) ? 1 : 0;
    }
    if (pos < n_args) {
        mp_raise_msg_varg(&mp_type_TypeError, "function: too many args");
    }

    if ((used_kws + 1) != (1 << kw_args->used)) {
        mp_raise_msg_varg(&mp_type_TypeError, "function: too many keywords");
    }
}

void parse_args_and_kw_map(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args, const char *format, const qstr keywords[], ...) {
    va_list vals;
    va_start(vals, keywords);
    vparse_args_and_kw(n_args, args, kw_args, format, keywords, vals);
    va_end(vals);
}

void parse_args_and_kw(size_t n_args, size_t n_kw, const mp_obj_t *args, const char *format, const qstr keywords[], ...) {
    va_list vals;
    va_start(vals, keywords);
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    vparse_args_and_kw(n_args, args, &kw_args, format, keywords, vals);
    va_end(vals);
}

void parse_args(size_t n_args, const mp_obj_t *args, const char *format, ...) {
    va_list vals;
    va_start(vals, format);
    const qstr keywords[] = { 0 };
    vparse_args_and_kw(n_args, args, NULL, format, keywords, vals);
    va_end(vals);
}
