// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"

#include "./queue.h"

#include "py/gc_handle.h"
#include "py/obj.h"

struct lvgl_obj;

typedef struct lvgl_handle {
    int ref_count;
    lv_obj_t *lv_obj;
    struct lvgl_obj *mp_obj;
    const mp_obj_type_t *type;
} lvgl_handle_t;

typedef struct lvgl_obj {
    mp_obj_base_t base;
    lvgl_handle_t *handle;
    mp_map_t members;
    mp_obj_base_t children;
} lvgl_obj_t;

typedef struct {
    lvgl_queue_elem_t elem;
    gc_handle_t *func;
    lvgl_handle_t *target;
    lv_event_code_t code;
} lvgl_obj_event_t;


// lv_obj_t *lvgl_lock_obj(lvgl_obj_t *self);

lvgl_obj_t *lvgl_obj_get(mp_obj_t self_in);

lvgl_handle_t *lvgl_obj_get_handle(lv_obj_t *obj);

lvgl_obj_t *lvgl_handle_get_obj(lvgl_handle_t *handle);

mp_obj_t lvgl_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *arg);

void lvgl_obj_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);

extern const mp_obj_type_t lvgl_type_obj;

void lvgl_obj_attr_int(lvgl_obj_t *self, qstr attr, int32_t (*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, int32_t value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest);

void lvgl_obj_attr_str(lvgl_obj_t *self, qstr attr, char *(*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, const char *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest);

void lvgl_obj_attr_obj(lvgl_obj_t *self, qstr attr, lv_obj_t *(*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, lv_obj_t *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest);

extern const mp_obj_type_t lvgl_type_obj_list;
