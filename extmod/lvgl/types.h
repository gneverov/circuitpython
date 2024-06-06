// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "./types/common.h"
#include "./types/shared_ptr.h"
#include "./types/static_ptr.h"


typedef enum lv_type_code {
    LV_TYPE_NONE,
    LV_TYPE_INT8,
    LV_TYPE_INT16,
    LV_TYPE_INT32,
#if LV_USE_FLOAT
    LV_TYPE_FLOAT,
#else
    LV_TYPE_FLOAT = LV_TYPE_INT32,
#endif
    LV_TYPE_COLOR,
    LV_TYPE_STR,
    LV_TYPE_AREA,
    LV_TYPE_POINT,
    LV_TYPE_POINT_PRECISE,
    
    LV_TYPE_ANIM,
    LV_TYPE_DRAW_BUFFER,
    LV_TYPE_GRAD_DSC,
    LV_TYPE_STYLE_TRANSITION_DSC,

    LV_TYPE_ANIM_PATH ,
    LV_TYPE_COLOR_FILTER,
    LV_TYPE_FONT,

    LV_TYPE_IMAGE_SRC,
    LV_TYPE_OBJ_HANDLE,
    LV_TYPE_PROP_LIST,
    LV_TYPE_GC_HANDLE,

    LV_TYPE_MAX,
} lv_type_code_t;

static_assert(sizeof(lv_type_code_t) == 1);

void lvgl_type_free(lv_type_code_t type_code, void *value);

void lvgl_type_from_mp(lv_type_code_t type_code, mp_obj_t obj, void *value);

mp_obj_t lvgl_type_to_mp(lv_type_code_t type_code, const void *value);

void lvgl_type_clone(lv_type_code_t type_code, void *dst, const void *src);

void lvgl_type_from_mp_array(lv_type_code_t type_code, mp_obj_t obj, size_t *array_size, void **parray);
mp_obj_t lvgl_type_to_mp_array(lv_type_code_t type_code, size_t array_size, const void *array);
void lvgl_type_clone_array(lv_type_code_t type_code, size_t array_size, void **dst, const void *src);

void lvgl_attrs_free(const lvgl_type_attr_t *attrs, void *value);

void lvgl_type_attr(qstr attr, mp_obj_t *dest, lv_type_code_t type_code, void *value);

bool lvgl_attrs_attr(qstr attr, mp_obj_t *dest, const lvgl_type_attr_t *attrs, void *value);

uint lvgl_bitfield_attr_bool(qstr attr, mp_obj_t *dest, uint value);

uint lvgl_bitfield_attr_int(qstr attr, mp_obj_t *dest, uint value);
