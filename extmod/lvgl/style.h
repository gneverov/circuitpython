// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "./types.h"
#include "py/qstr.h"


void lvgl_style_init(void);

lv_style_prop_t lvgl_style_lookup(qstr qstr, lv_type_code_t *type);

void lvgl_style_value_free(lv_type_code_t type, lv_style_value_t value);

lv_style_value_t lvgl_style_value_from_mp(lv_type_code_t type, mp_obj_t obj);

mp_obj_t lvgl_style_value_to_mp(lv_type_code_t type, lv_style_value_t value);


typedef struct lvgl_style_handle {
    lvgl_ptr_handle_t base;
    lv_style_t style;
} lvgl_style_handle_t;

extern const mp_obj_type_t lvgl_type_style;

extern const lvgl_ptr_type_t lvgl_style_type;

inline lvgl_style_handle_t *lvgl_style_get(mp_obj_t self_in) {
    return lvgl_ptr_from_mp(&lvgl_style_type, self_in);
}

inline lvgl_style_handle_t *lvgl_style_get_handle(const lv_style_t *style) {
    return lvgl_ptr_from_lv(&lvgl_style_type, style);
}

// extern const mp_obj_type_t lvgl_type_grad_dsc;

// extern const lvgl_struct_type_t lvgl_grad_dsc_type;

extern const mp_obj_type_t lvgl_type_style_transition_dsc;

extern const lvgl_ptr_type_t lvgl_style_transition_dsc_type;
