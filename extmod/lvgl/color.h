// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "./types.h"
#include "py/runtime.h"


typedef struct lvgl_obj_palette {
    mp_obj_base_t base;
    lv_palette_t p;
} lvgl_obj_palette_t;

extern const mp_obj_type_t lvgl_type_palette;


extern const mp_obj_type_t lvgl_type_color_filter;

extern const lvgl_static_ptr_type_t lvgl_color_filter_type;
