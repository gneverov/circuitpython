// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <string.h>

#include "py/gc.h"
#include "py/runtime.h"
#include "./freeze.h"


#if MICROPY_PY_FREEZE
MP_DEFINE_CONST_FUN_OBJ_0(freeze_clear_obj, freeze_clear);

MP_DEFINE_CONST_FUN_OBJ_KW(freeze_import_modules_obj, 0, freeze_import_modules);

MP_DEFINE_CONST_FUN_OBJ_0(freeze_get_modules_obj, freeze_get_modules);

static const mp_rom_map_elem_t freeze_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_freeze) },
    { MP_ROM_QSTR(MP_QSTR_import_modules),  MP_ROM_PTR(&freeze_import_modules_obj) },
    { MP_ROM_QSTR(MP_QSTR_clear),           MP_ROM_PTR(&freeze_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_modules),     MP_ROM_PTR(&freeze_get_modules_obj) },  
};

static MP_DEFINE_CONST_DICT(freeze_module_globals, freeze_module_globals_table);

const mp_obj_module_t freeze_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&freeze_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_freeze, freeze_module);
#endif
