// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "./obj.h"

#include "py/obj.h"


const mp_obj_type_t *lvgl_obj_class_from(const lv_obj_class_t *type_obj);
