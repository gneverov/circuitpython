// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "./obj.h"
#include "./types.h"


typedef struct lvgl_anim_handle {
    lvgl_ptr_handle_t base;
    lv_anim_t anim;
    lvgl_obj_handle_t *var;
    lv_style_prop_t *props;
    gc_handle_t *mp_exec_cb;
} lvgl_anim_handle_t;

typedef struct {
    lvgl_queue_elem_t elem;
    lvgl_anim_handle_t *handle;
    int32_t value;
} lvgl_anim_event_t;

extern const mp_obj_type_t lvgl_type_anim;
extern const lvgl_ptr_type_t lvgl_anim_type;

extern const mp_obj_type_t lvgl_type_anim_path;
extern const lvgl_static_ptr_type_t lvgl_anim_path_type;
