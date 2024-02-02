// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./modlvgl.h"
#include "./obj_class.h"
#include "./widgets.h"


static const struct lvgl_class {
    const lv_obj_class_t *type_obj;
    const mp_obj_type_t *type;
} lvgl_class_table[] = {
    { &lv_obj_class, &lvgl_type_obj },
    { &lv_button_class, &lvgl_type_button },
    { &lv_label_class, &lvgl_type_label },
    { &lv_slider_class, &lvgl_type_slider },
    { NULL, NULL },
};

static const mp_obj_type_t *lvgl_class_lookup(const lv_obj_class_t *type_obj) {
    const struct lvgl_class *elem = lvgl_class_table;
    for (; elem->type_obj; elem++) {
        if (elem->type_obj == type_obj) {
            return elem->type;
        }
    }
    return &lvgl_type_obj;
}

const mp_obj_type_t *lvgl_obj_class_from(const lv_obj_class_t *type_obj) {
    assert(lvgl_is_locked());
    return lvgl_class_lookup(type_obj);
}
