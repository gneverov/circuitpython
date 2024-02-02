// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"

#include "py/obj.h"

struct lvgl_obj_indev;

typedef struct lvgl_handle_indev {
    int ref_count;
    lv_indev_t *lv_indev;
    struct lvgl_obj_indev *mp_indev;
    const mp_obj_type_t *type;
    void (*deinit_cb)(lv_indev_t *indev);
} lvgl_handle_indev_t;

typedef struct lvgl_obj_indev {
    mp_obj_base_t base;
    lvgl_handle_indev_t *handle;
} lvgl_obj_indev_t;

lvgl_handle_indev_t *lvgl_handle_alloc_indev(lv_indev_t *indev, const mp_obj_type_t *type, void (*deinit_cb)(lv_indev_t *indev));

lvgl_obj_indev_t *lvgl_handle_get_indev(lvgl_handle_indev_t *handle);

extern const mp_obj_type_t lvgl_type_indev;
