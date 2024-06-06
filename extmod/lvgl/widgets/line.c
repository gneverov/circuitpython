// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./line.h"
#include "../obj.h"
#include "../super.h"
#include "../types.h"


__attribute__ ((visibility("hidden")))
void lvgl_line_event_delete(lv_obj_t *obj) {
    assert(lvgl_is_locked());
    lv_line_t *line = (void *)obj;
    lv_free((void *)line->point_array);
}

STATIC void lvgl_line_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);

    if (attr == MP_QSTR_points) {
        lvgl_super_attr_check(attr, true, true, false, dest);
        size_t num_new_points = 0;
        lv_point_precise_t *new_points = NULL;
        if (dest[1] != MP_OBJ_NULL) {
            lvgl_type_from_mp_array(LV_TYPE_POINT_PRECISE, dest[1], &num_new_points, (void *)&new_points);
        }

        lvgl_lock();
        lv_obj_t *obj = lvgl_lock_obj(handle);
        lv_line_t *line = (void *)obj;
        size_t num_old_points = line->point_num;
        const lv_point_precise_t *old_points = line->point_array;
        if (dest[0] != MP_OBJ_SENTINEL) {
            lv_point_t *tmp = NULL;
            lvgl_type_clone_array(LV_TYPE_POINT_PRECISE, num_old_points, (void *)&tmp, old_points);
            lvgl_unlock();
            dest[0] = lvgl_type_to_mp_array(LV_TYPE_POINT_PRECISE, num_old_points, tmp);
            lv_free(tmp);
        }
        else if (dest[1] != MP_OBJ_NULL) {
            lv_line_set_points(obj, new_points, num_new_points);
            lvgl_unlock();
            dest[0] = MP_OBJ_NULL;
            lv_free((void *)old_points);
        }
    }
    else {
        lvgl_super_attr(self_in, &lvgl_type_line, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_line,
    MP_ROM_QSTR_CONST(MP_QSTR_Line),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_line_attr,
    subscr, lvgl_obj_subscr,
    parent, &lvgl_type_obj,
    protocol, &lv_line_class
    );
MP_REGISTER_OBJECT(lvgl_type_line);
