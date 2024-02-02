// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "py/qstr.h"

void lvgl_style_init(void);

lv_style_prop_t lvgl_style_lookup(qstr qstr);
