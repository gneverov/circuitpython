// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./layer.h"

#include "py/runtime.h"


lv_layer_t *lvgl_layer_get(mp_obj_t obj_in) {
    const mp_obj_type_t *type = mp_obj_get_type(obj_in);
    if (!mp_obj_is_subclass_fast(type, MP_OBJ_FROM_PTR(&lvgl_type_layer))) {
        mp_raise_TypeError(NULL);
    }
    lvgl_obj_layer_t *obj = MP_OBJ_TO_PTR(obj_in);
    if (!obj->layer) {
        mp_raise_ValueError(MP_ERROR_TEXT("layer invalid"));
    }
    return obj->layer;
}

void lvgl_layer_init(lvgl_obj_layer_t *self, const mp_obj_type_t *type) {
    self->base.type = type;
    self->layer = NULL;
}

STATIC mp_obj_t lvgl_layer_del(mp_obj_t self_in) {
    lvgl_obj_layer_t *self = MP_OBJ_TO_PTR(self_in);
    assert(self->layer == NULL);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_layer_del_obj, lvgl_layer_del);

// STATIC void lvgl_layer_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
// }

STATIC const mp_rom_map_elem_t lvgl_layer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_layer_del_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_layer_locals_dict, lvgl_layer_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_layer,
    MP_ROM_QSTR_CONST(MP_QSTR_Layer),
    MP_TYPE_FLAG_NONE,
    // attr, lvgl_layer_attr,
    locals_dict, &lvgl_layer_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_layer);
