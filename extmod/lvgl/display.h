// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"

#include "py/obj.h"

struct lvgl_obj_display;

typedef struct lvgl_handle_display {
    int ref_count;
    lv_display_t *lv_disp;
    struct lvgl_obj_display *mp_disp;
    const mp_obj_type_t *type;
    void (*deinit_cb)(lv_display_t *disp);
    void *buf[2];
} lvgl_handle_display_t;

typedef struct lvgl_obj_display {
    mp_obj_base_t base;
    lvgl_handle_display_t *handle;
} lvgl_obj_display_t;

lvgl_handle_display_t *lvgl_handle_alloc_display(lv_display_t *disp, const mp_obj_type_t *type, void (*deinit_cb)(lv_display_t *disp));

bool lvgl_handle_alloc_display_draw_buffers(lvgl_handle_display_t *handle, size_t buf_size);

lvgl_obj_display_t *lvgl_handle_get_display(lvgl_handle_display_t *handle);

void lvgl_display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);

mp_obj_t lvgl_display_get_default(void);

extern const mp_obj_type_t lvgl_type_display;
