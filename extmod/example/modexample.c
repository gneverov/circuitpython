// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdio.h>

#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"


static const MP_DEFINE_STR_OBJ(example_str_obj, "hello");

static mp_obj_t example_func(mp_obj_t obj_in) {
    const char *str = mp_obj_str_get_str(obj_in);
    printf("Hello %s\n", str);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(example_func_obj, example_func);

static const mp_rom_map_elem_t example_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),    MP_ROM_QSTR(MP_QSTR_example) },
    { MP_ROM_QSTR(MP_QSTR_STR),        MP_ROM_PTR(&example_str_obj) },
    { MP_ROM_QSTR(MP_QSTR_func),       MP_ROM_PTR(&example_func_obj) },
};
static MP_DEFINE_CONST_DICT(example_module_globals, example_module_globals_table);

const mp_obj_module_t example_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&example_module_globals,
};
MP_REGISTER_MODULE(MP_QSTR_example, example_module);
MP_REGISTER_OBJECT(example_module);
