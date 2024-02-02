// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./obj.h"
#include "./super.h"
#include "./widgets.h"


MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_button,
    MP_QSTR_Button,
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_obj_attr,
    parent, &lvgl_type_obj,
    protocol, &lv_button_class
    );


STATIC void lvgl_label_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_t *self = lvgl_obj_get(self_in);

    if (attr == MP_QSTR_text) {
        lvgl_obj_attr_str(self, attr, lv_label_get_text, lv_label_set_text, NULL, dest);
    }
    else {
        lvgl_super_attr(self_in, &lvgl_type_label, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_label,
    MP_QSTR_Label,
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_label_attr,
    parent, &lvgl_type_obj,
    protocol, &lv_label_class
    );


STATIC void lv_slider_set_value_0(lv_obj_t * obj, int32_t value) {
    lv_slider_set_value(obj, value, LV_ANIM_OFF);
}

STATIC void lvgl_slider_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_t *self = lvgl_obj_get(self_in);

    if (attr == MP_QSTR_value) {
        lvgl_obj_attr_int(self, attr, lv_slider_get_value, lv_slider_set_value_0, NULL, dest);
    }
    else {
        lvgl_super_attr(self_in, &lvgl_type_label, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_slider,
    MP_QSTR_Slider,
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_slider_attr,
    parent, &lvgl_type_obj,
    protocol, &lv_slider_class
    );
