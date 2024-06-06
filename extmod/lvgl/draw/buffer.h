// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "../types.h"
#include "py/obj.h"


typedef struct lvgl_draw_buf_handle {
    lvgl_ptr_handle_t base;
} lvgl_draw_buf_handle_t;

extern const mp_obj_type_t lvgl_type_draw_buf;

extern const lvgl_ptr_type_t lvgl_draw_buf_type;

lvgl_draw_buf_handle_t *lvgl_draw_buf_from_lv(const lv_draw_buf_t *draw_buf);

inline lv_draw_buf_t *lvgl_draw_buf_to_lv(lvgl_draw_buf_handle_t *handle) {
    return lvgl_ptr_to_lv(&handle->base);
}
