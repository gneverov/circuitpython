// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "py/obj.h"


typedef struct lvgl_type_attr {
    qstr_short_t qstr;
    uint8_t offset;
    uint8_t type_code;
} lvgl_type_attr_t;
