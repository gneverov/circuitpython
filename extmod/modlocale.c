// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <langinfo.h>
#include <locale.h>
#include <string.h>

#include "py/parseargs.h"
#include "py/runtime.h"


static MP_DEFINE_CONST_OBJ_TYPE(
    locale_type_Error,
    MP_QSTR_Error,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new,
    print, mp_obj_exception_print,
    attr, mp_obj_exception_attr,
    parent, &mp_type_Exception
    );

static mp_obj_t locale_mkstr(const char *str) {
    if (!str) {
        mp_raise_type(&locale_type_Error);
    }
    return mp_obj_new_str(str, strlen(str));
}

static mp_obj_t locale_getlocale(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_category, 0 };
    int category = LC_CTYPE;
    parse_args_and_kw_map(n_args, args, kw_args, "|i", kws, &category);
    return locale_mkstr(setlocale(category, NULL));
}
MP_DEFINE_CONST_FUN_OBJ_KW(locale_getlocale_obj, 0, locale_getlocale);

static mp_obj_t locale_setlocale(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_category, MP_QSTR_locale, 0 };
    int category;
    const char *locale = NULL;
    parse_args_and_kw_map(n_args, args, kw_args, "i|z", kws, &category, &locale);
    return locale_mkstr(setlocale(category, locale));
}
MP_DEFINE_CONST_FUN_OBJ_KW(locale_setlocale_obj, 1, locale_setlocale);

static const mp_rom_map_elem_t locale_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_locale) },
    { MP_ROM_QSTR(MP_QSTR_Error),       MP_ROM_PTR(&locale_type_Error) },
    { MP_ROM_QSTR(MP_QSTR_getlocale),   MP_ROM_PTR(&locale_getlocale_obj) },
    { MP_ROM_QSTR(MP_QSTR_setlocale),   MP_ROM_PTR(&locale_setlocale_obj) },

    { MP_ROM_QSTR(MP_QSTR_LC_ALL),      MP_ROM_INT(LC_ALL) },
    { MP_ROM_QSTR(MP_QSTR_LC_COLLATE),  MP_ROM_INT(LC_COLLATE) },
    { MP_ROM_QSTR(MP_QSTR_LC_CTYPE),    MP_ROM_INT(LC_CTYPE) },
    { MP_ROM_QSTR(MP_QSTR_LC_MONETARY), MP_ROM_INT(LC_MONETARY) },
    { MP_ROM_QSTR(MP_QSTR_LC_NUMERIC),  MP_ROM_INT(LC_NUMERIC) },
    { MP_ROM_QSTR(MP_QSTR_LC_TIME),     MP_ROM_INT(LC_TIME) },
    { MP_ROM_QSTR(MP_QSTR_LC_MESSAGES), MP_ROM_INT(LC_MESSAGES) },
};

static MP_DEFINE_CONST_DICT(locale_module_globals, locale_module_globals_table);

const mp_obj_module_t mp_module_locale = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&locale_module_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_locale, mp_module_locale);
