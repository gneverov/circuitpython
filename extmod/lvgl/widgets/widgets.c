// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "../obj.h"
#include "../super.h"
#include "./widgets.h"


MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_button,
    MP_ROM_QSTR_CONST(MP_QSTR_Button),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_obj_attr,
    subscr, lvgl_obj_subscr,
    parent, &lvgl_type_obj,
    protocol, &lv_button_class
    );
MP_REGISTER_OBJECT(lvgl_type_button);


static int32_t lv_label_get_long_int(lv_obj_t *obj) {
    return lv_label_get_long_mode(obj);
} 

static void lv_label_set_long_int(lv_obj_t *obj, int32_t value) {
    lv_label_set_long_mode(obj, value);
}

static void lvgl_label_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);

    if (attr == MP_QSTR_text) {
        lvgl_obj_attr_str(handle, attr, lv_label_get_text, lv_label_set_text, NULL, dest);
    }
    else if (attr == MP_QSTR_long_mode) {
        lvgl_obj_attr_int(handle, attr, lv_label_get_long_int, lv_label_set_long_int, NULL, dest);
    }
    else {
        lvgl_super_attr(self_in, &lvgl_type_label, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_label,
    MP_ROM_QSTR_CONST(MP_QSTR_Label),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_label_attr,
    subscr, lvgl_obj_subscr,
    parent, &lvgl_type_obj,
    protocol, &lv_label_class
    );
MP_REGISTER_OBJECT(lvgl_type_label);


static void lv_slider_set_value_0(lv_obj_t * obj, int32_t value) {
    lv_slider_set_value(obj, value, LV_ANIM_OFF);
}

static void lv_slider_set_left_value_0(lv_obj_t * obj, int32_t left_value) {
    lv_slider_set_left_value(obj, left_value, LV_ANIM_OFF);
}

static void lv_slider_set_min_value(lv_obj_t * obj, int32_t min_value) {
    lv_slider_set_range(obj, min_value, lv_slider_get_max_value(obj));
}

static void lv_slider_set_max_value(lv_obj_t * obj, int32_t max_value) {
    lv_slider_set_range(obj, lv_slider_get_min_value(obj), max_value);
}

static void lvgl_slider_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);

    if (attr == MP_QSTR_value) {
        lvgl_obj_attr_int(handle, attr, lvgl_obj_attr_int_const(lv_slider_get_value), lv_slider_set_value_0, NULL, dest);
    }
    else if (attr == MP_QSTR_left_value) {
        lvgl_obj_attr_int(handle, attr, lvgl_obj_attr_int_const(lv_slider_get_left_value), lv_slider_set_left_value_0, NULL, dest);
    }
    else if (attr == MP_QSTR_min_value) {
        lvgl_obj_attr_int(handle, attr, lvgl_obj_attr_int_const(lv_slider_get_min_value), lv_slider_set_min_value, NULL, dest);
    }
    else if (attr == MP_QSTR_max_value) {
        lvgl_obj_attr_int(handle, attr, lvgl_obj_attr_int_const(lv_slider_get_max_value), lv_slider_set_max_value, NULL, dest);
    }
    else {
        lvgl_super_attr(self_in, &lvgl_type_label, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_slider,
    MP_ROM_QSTR_CONST(MP_QSTR_Slider),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_slider_attr,
    subscr, lvgl_obj_subscr,
    parent, &lvgl_type_obj,
    protocol, &lv_slider_class
    );
MP_REGISTER_OBJECT(lvgl_type_slider);


MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_switch,
    MP_ROM_QSTR_CONST(MP_QSTR_Switch),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_obj_attr,
    subscr, lvgl_obj_subscr,
    parent, &lvgl_type_obj,
    protocol, &lv_switch_class
    );
MP_REGISTER_OBJECT(lvgl_type_switch);
