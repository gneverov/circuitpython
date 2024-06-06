// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./modlvgl.h"
#include "./obj_class.h"
#include "./widgets/arc.h"
#include "./widgets/canvas.h"
#include "./widgets/image.h"
#include "./widgets/line.h"
#include "./widgets/widgets.h"


static const struct lvgl_class lvgl_class_table[] = {
    { &lv_obj_class,    &lvgl_type_obj,     NULL },
    { &lv_arc_class,    &lvgl_type_arc,     NULL },
    { &lv_button_class, &lvgl_type_button,  NULL },
    { &lv_canvas_class, &lvgl_type_canvas,  lvgl_canvas_event_delete },
    { &lv_image_class,  &lvgl_type_image,   lvgl_image_event_delete },
    { &lv_label_class,  &lvgl_type_label,   NULL },
    { &lv_line_class,   &lvgl_type_line,    lvgl_line_event_delete },
    { &lv_slider_class, &lvgl_type_slider,  NULL },
    { &lv_switch_class, &lvgl_type_switch,  NULL },
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
