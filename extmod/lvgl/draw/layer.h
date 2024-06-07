// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"

#include "py/obj.h"


typedef struct lvgl_obj_layer {
    mp_obj_base_t base;
    lv_layer_t *layer;
} lvgl_obj_layer_t;

void lvgl_layer_init(lvgl_obj_layer_t *self);

lv_layer_t *lvgl_layer_get(mp_obj_t obj_in);

extern const mp_obj_type_t lvgl_type_layer;
