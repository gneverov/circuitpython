// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "./types/shared_ptr.h"
#include "py/obj.h"


typedef struct lvgl_display_handle {
    lvgl_ptr_handle_t base;
    void (*deinit_cb)(lv_display_t *disp);
    void *buf[2];
} lvgl_display_handle_t;

lvgl_display_handle_t *lvgl_display_alloc_handle(lv_display_t *disp, void (*deinit_cb)(lv_display_t *));

bool lvgl_display_alloc_draw_buffers(lvgl_display_handle_t *handle, size_t buf_size);

void lvgl_display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);

mp_obj_t lvgl_display_get_default(void);

extern const mp_obj_type_t lvgl_type_display;

extern const lvgl_ptr_type_t lvgl_display_type;
