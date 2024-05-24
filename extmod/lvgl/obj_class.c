// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./modlvgl.h"
#include "./canvas.h"
#include "./obj_class.h"
#include "./widgets.h"


static const struct lvgl_class lvgl_class_table[] = {
    { &lv_obj_class, &lvgl_type_obj },
    { &lv_button_class, &lvgl_type_button },
    { &lv_canvas_class, &lvgl_type_canvas },
    { &lv_label_class, &lvgl_type_label },
    { &lv_slider_class, &lvgl_type_slider },
    { &lv_switch_class, &lvgl_type_switch },
    { NULL, NULL },
};

const lvgl_class_t *lvgl_class_lookup(const lv_obj_class_t *lv_class) {
    const struct lvgl_class *elem = lvgl_class_table;
    for (; elem->lv_class; elem++) {
        if (elem->lv_class == lv_class) {
            return elem;
        }
    }
    return &lvgl_class_table[0];
}
