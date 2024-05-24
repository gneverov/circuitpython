// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "./obj.h"
#include "./types.h"


typedef struct lvgl_anim_handle {
    lvgl_ptr_handle_t base;
    lv_anim_t anim;
    lvgl_handle_t *var;
    lv_style_prop_t *props;
} lvgl_anim_handle_t;

// lvgl_anim_handle_t *lvgl_anim_get_handle(const void *anim);

extern const mp_obj_type_t lvgl_type_anim;
extern const lvgl_ptr_type_t lvgl_anim_type;

extern const mp_obj_type_t lvgl_type_anim_path;
extern const lvgl_static_ptr_type_t lvgl_anim_path_type;
