// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "py/obj.h"


typedef struct lvgl_static_ptr_type {
    const mp_obj_type_t *mp_type;
    const mp_map_t *map;
    // const struct lvgl_type_attr *attrs;
} lvgl_static_ptr_type_t;

typedef struct lvgl_obj_static_ptr {
    mp_obj_base_t base;
    const void *lv_ptr;
} lvgl_obj_static_ptr_t;

const void *lvgl_static_ptr_from_mp(const lvgl_static_ptr_type_t *type, mp_obj_t obj_in);

mp_obj_t lvgl_static_ptr_to_mp(const lvgl_static_ptr_type_t *type, const void *ptr);
