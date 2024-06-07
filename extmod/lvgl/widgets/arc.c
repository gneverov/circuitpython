// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./arc.h"
#include "../obj.h"
#include "../super.h"


static void lvgl_arc_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);

    if (attr == MP_QSTR_start_angle) {
        lvgl_obj_attr_int(handle, attr, lv_arc_get_angle_start, lv_arc_set_start_angle, NULL, dest);
    }
    else if (attr == MP_QSTR_end_angle) {
        lvgl_obj_attr_int(handle, attr, lv_arc_get_angle_end, lv_arc_set_end_angle, NULL, dest);
    }
    else if (attr == MP_QSTR_bg_start_angle) {
        lvgl_obj_attr_int(handle, attr, lv_arc_get_bg_angle_start, lv_arc_set_bg_start_angle, NULL, dest);
    }
    else if (attr == MP_QSTR_bg_end_angle) {
        lvgl_obj_attr_int(handle, attr, lv_arc_get_bg_angle_end, lv_arc_set_bg_end_angle, NULL, dest);
    } 
    else if (attr == MP_QSTR_rotation) {
        lvgl_obj_attr_int(handle, attr, lvgl_obj_attr_int_const(lv_arc_get_rotation), lv_arc_set_rotation, NULL, dest);
    } 
    else if (attr == MP_QSTR_value) {
        lvgl_obj_attr_int(handle, attr, lvgl_obj_attr_int_const(lv_arc_get_value), lv_arc_set_value, NULL, dest);
    }               
    else {
        lvgl_super_attr(self_in, &lvgl_type_arc, attr, dest);
    }
}

static mp_obj_t lvgl_arc_rotate_obj_to_angle(mp_obj_t self_in, mp_obj_t obj_in, mp_obj_t r_offset_in) {
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);
    lvgl_obj_handle_t *obj_handle = lvgl_obj_from_mp_checked(obj_in);
    int32_t r_offset = mp_obj_get_int(r_offset_in);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_obj_t *obj_to_rotate = lvgl_lock_obj(obj_handle);
    lv_arc_rotate_obj_to_angle(obj, obj_to_rotate, r_offset);
    lvgl_unlock();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(lvgl_arc_rotate_obj_to_angle_obj, lvgl_arc_rotate_obj_to_angle);

static const mp_rom_map_elem_t lvgl_arc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_rotate_obj_to_angle),     MP_ROM_PTR(&lvgl_arc_rotate_obj_to_angle_obj) },

};
static MP_DEFINE_CONST_DICT(lvgl_arc_locals_dict, lvgl_arc_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_arc,
    MP_ROM_QSTR_CONST(MP_QSTR_Arc),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_arc_attr,
    subscr, lvgl_obj_subscr,
    parent, &lvgl_type_obj,
    protocol, &lv_arc_class,
    locals_dict, &lvgl_arc_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_arc);
