// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"

#include "py/runtime.h"


typedef struct lvgl_dsc_type {
    lvgl_ptr_type_t base;
    size_t size;
    void (*init)(lv_draw_dsc_base_t *dsc);
    void (*draw)(lv_layer_t *layer, lv_draw_dsc_base_t *dsc);
    void (*draw_coords)(lv_layer_t *layer, lv_draw_dsc_base_t *dsc, const lv_area_t *coords);
} lvgl_dsc_type_t;

extern const mp_obj_type_t lvgl_type_dsc;
extern const mp_obj_type_t lvgl_type_arc_dsc;
extern const mp_obj_type_t lvgl_type_image_dsc;
extern const mp_obj_type_t lvgl_type_label_dsc;
extern const mp_obj_type_t lvgl_type_line_dsc;
extern const mp_obj_type_t lvgl_type_rect_dsc;

extern const lvgl_dsc_type_t lvgl_arc_dsc_type;
extern const lvgl_dsc_type_t lvgl_image_dsc_type;
extern const lvgl_dsc_type_t lvgl_label_dsc_type;
extern const lvgl_dsc_type_t lvgl_line_dsc_type;
extern const lvgl_dsc_type_t lvgl_rect_dsc_type;
