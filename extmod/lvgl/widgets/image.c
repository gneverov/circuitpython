// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./image.h"
#include "../obj.h"
#include "../super.h"
#include "../types.h"


__attribute__ ((visibility("hidden")))
void lvgl_image_event_delete(lv_obj_t *obj) {
    assert(lvgl_is_locked());
    const void *src = lv_image_get_src(obj);
    lvgl_type_free(LV_TYPE_IMAGE_SRC, &src);
}

static void lv_image_set_scale_x_0(lv_obj_t *obj, int32_t value) {
    lv_image_set_scale_x(obj, value);
}

static void lv_image_set_scale_y_0(lv_obj_t *obj, int32_t value) {
    lv_image_set_scale_y(obj, value);
}

static void lvgl_image_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);

    if (attr == MP_QSTR_src) {
        lvgl_super_attr_check(attr, true, true, false, dest);
        const void *new_src = NULL;
        if (dest[1] != MP_OBJ_NULL) {
            lvgl_type_from_mp(LV_TYPE_IMAGE_SRC, dest[1], &new_src);
        }

        lvgl_lock();
        lv_obj_t *obj = lvgl_lock_obj(handle);
        const void *old_src = lv_image_get_src(obj);
        if (dest[0] != MP_OBJ_SENTINEL) {
            const void *tmp = NULL;
            lvgl_type_clone(LV_TYPE_IMAGE_SRC, &tmp, &old_src);
            lvgl_unlock();
            dest[0] = lvgl_type_to_mp(LV_TYPE_IMAGE_SRC, &tmp);
            lvgl_type_free(LV_TYPE_IMAGE_SRC, &tmp);
        }
        else if (dest[1] != MP_OBJ_NULL) {
            lv_image_set_src(obj, new_src);
            lvgl_unlock();
            dest[0] = MP_OBJ_NULL;
            lvgl_type_free(LV_TYPE_IMAGE_SRC, &old_src);
        }
    }
    else if (attr == MP_QSTR_scale_x) {
        lvgl_obj_attr_int(handle, attr, lv_image_get_scale_x, lv_image_set_scale_x_0, NULL, dest);
    }
    else if (attr == MP_QSTR_scale_y) {
        lvgl_obj_attr_int(handle, attr, lv_image_get_scale_y, lv_image_set_scale_y_0, NULL, dest);
    }
    else if (attr == MP_QSTR_offset_x) {
        lvgl_obj_attr_int(handle, attr, lv_image_get_offset_x, lv_image_set_offset_x, NULL, dest);
    }
    else if (attr == MP_QSTR_offset_y) {
        lvgl_obj_attr_int(handle, attr, lv_image_get_offset_y, lv_image_set_offset_y, NULL, dest);
    }    
    else if (attr == MP_QSTR_rotation) {
        lvgl_obj_attr_int(handle, attr, lv_image_get_rotation, lv_image_set_rotation, NULL, dest);
    } 
    else if (attr == MP_QSTR_pivot) {
        lvgl_super_attr_check(attr, true, true, false, dest);
        lv_point_t value;
        if (dest[1] != MP_OBJ_NULL) {
            lvgl_type_from_mp(LV_TYPE_POINT, dest[1], &value);
        }
        
        lvgl_lock();
        lv_obj_t *obj = lvgl_lock_obj(handle);
        if (dest[0] != MP_OBJ_SENTINEL) {
            lv_image_get_pivot(obj, &value);
            lvgl_unlock();
            dest[0] = lvgl_type_to_mp(LV_TYPE_POINT, &value);
            return;
        }
        else if (dest[1] != MP_OBJ_NULL) {
            lv_image_set_pivot(obj, value.x, value.y);
            dest[0] = MP_OBJ_NULL;
        }
        lvgl_unlock();
    }    
    else {
        lvgl_super_attr(self_in, &lvgl_type_image, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_image,
    MP_ROM_QSTR_CONST(MP_QSTR_Image),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_image_attr,
    subscr, lvgl_obj_subscr,
    parent, &lvgl_type_obj,
    protocol, &lv_image_class
    );
MP_REGISTER_OBJECT(lvgl_type_image);
