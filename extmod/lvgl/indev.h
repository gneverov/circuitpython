// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "./types/shared_ptr.h"
#include "py/obj.h"


typedef struct lvgl_indev_handle {
    lvgl_ptr_handle_t base;
    void (*deinit_cb)(lv_indev_t *indev);
} lvgl_indev_handle_t;

lvgl_indev_handle_t *lvgl_indev_alloc_handle(lv_indev_t *indev, void (*deinit_cb)(lv_indev_t *));

extern const mp_obj_type_t lvgl_type_indev;

extern const lvgl_ptr_type_t lvgl_indev_type;

inline lvgl_indev_handle_t *lvgl_indev_get_handle(lv_indev_t *indev) {
    return lvgl_ptr_from_lv(&lvgl_indev_type, indev);
}

inline lv_indev_t *lvgl_indev_to_lv(lvgl_indev_handle_t *handle) {
    return lvgl_ptr_to_lv(&handle->base);
}

mp_obj_t lvgl_indev_get_active(void);
