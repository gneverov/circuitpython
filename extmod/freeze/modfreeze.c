// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <string.h>

#include "newlib/dlfcn.h"

#include "py/gc.h"
#include "py/runtime.h"
#include "./freeze.h"


#if MICROPY_PY_FREEZE
static mp_obj_t freeze_clear_py(void) {
    if (!freeze_clear()) {
        mp_raise_msg(&mp_type_RuntimeError, "Freezing in progress");
    }
    // Restart for changes to take effect
    exit(0);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(freeze_clear_obj, freeze_clear_py);

static mp_obj_t freeze_import_modules(size_t n_args, const mp_obj_t *args) {
    mp_obj_t result = freeze_import(n_args, args);
    if (result == MP_OBJ_NULL) {
        mp_raise_msg(&mp_type_RuntimeError, "Reboot pending");
    }
    return result;
}
MP_DEFINE_CONST_FUN_OBJ_VAR(freeze_import_modules_obj, 0, freeze_import_modules);

static mp_obj_t freeze_get_modules(void) {
    return freeze_modules();
}
MP_DEFINE_CONST_FUN_OBJ_0(freeze_get_modules_obj, freeze_get_modules);

static mp_obj_t freeze_flash_py(mp_obj_t file_in) {
    const char *file = mp_obj_str_get_str(file_in);
    freeze_flash(file);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(freeze_flash_obj, freeze_flash_py);

static const mp_rom_map_elem_t freeze_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_freeze) },
    { MP_ROM_QSTR(MP_QSTR_import_modules), MP_ROM_PTR(&freeze_import_modules_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&freeze_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_modules), MP_ROM_PTR(&freeze_get_modules_obj) },

    { MP_ROM_QSTR(MP_QSTR_flash), MP_ROM_PTR(&freeze_flash_obj) },
};

static MP_DEFINE_CONST_DICT(freeze_module_globals, freeze_module_globals_table);

const mp_obj_module_t freeze_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&freeze_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_freeze, freeze_module);
#endif
