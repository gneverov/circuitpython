// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "./obj.h"

#include "py/obj.h"


typedef void (*lvgl_obj_deinit_t)(lv_obj_t *);

typedef struct lvgl_class {
    const lv_obj_class_t *lv_class;
    const mp_obj_type_t *mp_type;
    lvgl_obj_deinit_t deinit_cb;
} lvgl_class_t;

const lvgl_class_t *lvgl_class_lookup(const lv_obj_class_t *type_obj);
