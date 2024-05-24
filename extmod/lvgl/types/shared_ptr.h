// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"
#include "py/obj.h"


typedef void *lvgl_ptr_t;

typedef struct lvgl_ptr_type {
    const mp_obj_type_t *mp_type;
    mp_obj_t (*new_mp)(lvgl_ptr_t);
    void (*delete)(lvgl_ptr_t);
    lvgl_ptr_t (*get_handle)(const void *);
    const struct lvgl_type_attr *attrs;
} lvgl_ptr_type_t;

typedef struct lvgl_ptr_handle {
    const struct lvgl_ptr_type *type;
    int ref_count;
    mp_obj_t mp_obj;
    void *lv_ptr;
} lvgl_ptr_handle_t;

typedef struct lvgl_obj_ptr {
    mp_obj_base_t base;
    struct lvgl_ptr_handle *handle;
} lvgl_obj_ptr_t;

void lvgl_ptr_init_handle(lvgl_ptr_handle_t *handle, const struct lvgl_ptr_type *type, void *lv_ptr);

void lvgl_ptr_init_obj(lvgl_obj_ptr_t *obj, const mp_obj_type_t *mp_type, lvgl_ptr_handle_t *handle);

lvgl_ptr_t lvgl_ptr_copy(lvgl_ptr_handle_t *handle);

void lvgl_ptr_delete(lvgl_ptr_handle_t *handle);

lvgl_ptr_t lvgl_ptr_from_mp(const lvgl_ptr_type_t *type, mp_obj_t obj_in);

mp_obj_t lvgl_ptr_to_mp(lvgl_ptr_handle_t *handle);

mp_obj_t lvgl_unlock_ptr(lvgl_ptr_handle_t *handle);

mp_obj_t lvgl_ptr_del(mp_obj_t self_in);
MP_DECLARE_CONST_FUN_OBJ_1(lvgl_ptr_del_obj);

void lvgl_ptr_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);

lvgl_ptr_t lvgl_ptr_from_lv(const lvgl_ptr_type_t *type, const void* lv_ptr);

inline void *lvgl_ptr_to_lv(lvgl_ptr_handle_t *handle) {
    return handle->lv_ptr;
}

inline void lvgl_ptr_reset(lvgl_ptr_handle_t *handle) {
    handle->lv_ptr = NULL;
}
