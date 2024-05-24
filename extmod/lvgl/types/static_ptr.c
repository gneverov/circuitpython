// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./static_ptr.h"
#include "py/runtime.h"


const void *lvgl_static_ptr_from_mp(const lvgl_static_ptr_type_t *type, mp_obj_t obj_in) {
    if (type && !mp_obj_is_exact_type(obj_in, MP_OBJ_FROM_PTR(type->mp_type))) {
        mp_raise_TypeError(NULL);
    }
    lvgl_obj_static_ptr_t *obj = MP_OBJ_TO_PTR(obj_in);
    return obj->lv_ptr;
}

mp_obj_t lvgl_static_ptr_to_mp(const lvgl_static_ptr_type_t *type, const void *ptr) {
    if (!ptr) {
        return mp_const_none;
    }    
    const mp_map_t *map = type->map;
    for (size_t i = 0; i < map->alloc; i++) {
        mp_obj_t obj_in = map->table[i].value;
        if (!mp_obj_is_exact_type(obj_in, MP_OBJ_FROM_PTR(type->mp_type))) {
            continue;
        }
        const lvgl_obj_static_ptr_t *obj = MP_OBJ_TO_PTR(obj_in);
        if (obj->lv_ptr == ptr) {
            return obj_in;
        }
    }

    mp_raise_ValueError(NULL);

    // lvgl_obj_static_ptr_t *obj = m_new_obj(lvgl_obj_static_ptr_t);
    // obj->base.type = type->mp_type;
    // obj->lv_ptr = ptr;
    // return MP_OBJ_FROM_PTR(obj);
}
