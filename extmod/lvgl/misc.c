// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./misc.h"
#include "extmod/freeze/extmod.h"
#include "py/runtime.h"


void lvgl_area_from_mp(mp_obj_t obj, lv_area_t *area) {
    if (!mp_obj_is_exact_type(obj, MP_OBJ_FROM_PTR(&mp_type_tuple)) && !mp_obj_is_exact_type(obj, MP_OBJ_FROM_PTR(&mp_type_attrtuple))) {
        mp_raise_TypeError(NULL);
    }
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(obj, &len, &items);
    if (len != 4) {
        mp_raise_TypeError(NULL);
    }
    area->x1 = mp_obj_get_int(items[0]);
    area->y1 = mp_obj_get_int(items[1]);
    area->x2 = mp_obj_get_int(items[2]);
    area->y2 = mp_obj_get_int(items[3]);
}

void lvgl_point_from_mp(mp_obj_t obj, lv_point_t *point) {
    if (!mp_obj_is_exact_type(obj, MP_OBJ_FROM_PTR(&mp_type_tuple)) && !mp_obj_is_exact_type(obj, MP_OBJ_FROM_PTR(&mp_type_attrtuple))) {
        mp_raise_TypeError(NULL);
    }
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(obj, &len, &items);
    if (len != 2) {
        mp_raise_TypeError(NULL);
    }
    point->x = mp_obj_get_int(items[0]);
    point->y = mp_obj_get_int(items[1]);
}

void lvgl_point_precise_from_mp(mp_obj_t obj, lv_point_precise_t *point) {
    if (!mp_obj_is_exact_type(obj, MP_OBJ_FROM_PTR(&mp_type_tuple)) && !mp_obj_is_exact_type(obj, MP_OBJ_FROM_PTR(&mp_type_attrtuple))) {
        mp_raise_TypeError(NULL);
    }
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(obj, &len, &items);
    if (len != 2) {
        mp_raise_TypeError(NULL);
    }
#if LV_USE_FLOAT    
    point->x = mp_obj_get_float(items[0]);
    point->y = mp_obj_get_float(items[1]);
#else
    point->x = mp_obj_get_int(items[0]);
    point->y = mp_obj_get_int(items[1]);
#endif
}

STATIC const qstr lvgl_area_attrs[] = { MP_ROM_QSTR_CONST(MP_QSTR_x1), MP_ROM_QSTR_CONST(MP_QSTR_y1), MP_ROM_QSTR_CONST(MP_QSTR_x2), MP_ROM_QSTR_CONST(MP_QSTR_y2) };
MP_REGISTER_STRUCT(lvgl_area_attrs, qstr);

mp_obj_t lvgl_area_to_mp(const lv_area_t *area) {
    mp_obj_t items[] = {
        mp_obj_new_int(area->x1),
        mp_obj_new_int(area->y1),
        mp_obj_new_int(area->x2),
        mp_obj_new_int(area->y2),
    };
    return mp_obj_new_attrtuple(lvgl_area_attrs, 4, items);
}

STATIC const qstr lvgl_point_attrs[] = { MP_ROM_QSTR_CONST(MP_QSTR_x), MP_ROM_QSTR_CONST(MP_QSTR_y) };
MP_REGISTER_STRUCT(lvgl_point_attrs, qstr);

mp_obj_t lvgl_point_to_mp(const lv_point_t *point) {    
    mp_obj_t items[] = {
        mp_obj_new_int(point->x),
        mp_obj_new_int(point->y),
    };
    return mp_obj_new_attrtuple(lvgl_point_attrs, 2, items);
}

mp_obj_t lvgl_point_precise_to_mp(const lv_point_precise_t *point) {
    mp_obj_t items[] = {
#if LV_USE_FLOAT
        mp_obj_new_float(point->x),
        mp_obj_new_float(point->y),
#else        
        mp_obj_new_int(point->x),
        mp_obj_new_int(point->y),
#endif
    };
    return mp_obj_new_attrtuple(lvgl_point_attrs, 2, items);
}
