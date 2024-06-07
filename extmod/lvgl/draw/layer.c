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

void lvgl_layer_init(lvgl_obj_layer_t *self) {
    self->layer = NULL;
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_layer,
    MP_ROM_QSTR_CONST(MP_QSTR_Layer),
    MP_TYPE_FLAG_NONE
    );
MP_REGISTER_OBJECT(lvgl_type_layer);
