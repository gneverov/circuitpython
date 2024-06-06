// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "py/obj.h"


extern const mp_obj_type_t lvgl_type_image;

void lvgl_image_event_delete(lv_obj_t *obj);
