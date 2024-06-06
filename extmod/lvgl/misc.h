// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "lvgl.h"
#include "py/obj.h"


void lvgl_area_from_mp(mp_obj_t obj, lv_area_t *area);

void lvgl_point_from_mp(mp_obj_t obj, lv_point_t *point);

void lvgl_point_precise_from_mp(mp_obj_t obj, lv_point_precise_t *point);

mp_obj_t lvgl_area_to_mp(const lv_area_t *area);

mp_obj_t lvgl_point_to_mp(const lv_point_t *point);

mp_obj_t lvgl_point_precise_to_mp(const lv_point_precise_t *point);
