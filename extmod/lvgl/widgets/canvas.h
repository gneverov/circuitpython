// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "../draw/layer.h"


typedef struct lvgl_obj_canvas_layer {
    lvgl_obj_layer_t base;
    mp_obj_t canvas;
    lv_layer_t layer;
} lvgl_obj_canvas_layer_t;

extern const mp_obj_type_t lvgl_type_canvas;

extern const mp_obj_type_t lvgl_type_canvas_layer;

void lvgl_canvas_event_delete(lv_obj_t *obj);
