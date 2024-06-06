// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lvgl.h"

#include "./obj_class.h"
#include "./modlvgl.h"
#include "./queue.h"
#include "./types/shared_ptr.h"

#include "py/gc_handle.h"
#include "py/obj.h"


typedef struct lvgl_obj_handle {
    lvgl_ptr_handle_t base;
} lvgl_obj_handle_t;

typedef struct lvgl_obj_part {
    lvgl_obj_ptr_t base;
    lv_style_selector_t selector;
    struct lvgl_obj *whole;
} lvgl_obj_part_t;

typedef struct lvgl_obj {
    lvgl_obj_part_t part;
    mp_map_t members;
    mp_obj_base_t children;
} lvgl_obj_t;

typedef struct {
    lvgl_queue_elem_t elem;
    gc_handle_t *func;
    lvgl_obj_handle_t *current_target;
    lvgl_obj_handle_t *target;
    lv_event_code_t code;
} lvgl_obj_event_t;


lv_obj_t *lvgl_lock_obj(lvgl_obj_handle_t *handle);

lvgl_obj_handle_t *lvgl_obj_from_mp(mp_obj_t self, lv_style_selector_t *selector);
lvgl_obj_handle_t *lvgl_obj_from_mp_checked(mp_obj_t self);

extern const lvgl_ptr_type_t lvgl_obj_type;

lvgl_obj_handle_t *lvgl_obj_copy(lvgl_obj_handle_t *handle);

lvgl_obj_handle_t *lvgl_obj_from_lv(lv_obj_t *obj);

mp_obj_t lvgl_obj_to_mp(lvgl_obj_handle_t *handle);

mp_obj_t lvgl_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *arg);

void lvgl_obj_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest);
mp_obj_t lvgl_obj_subscr(mp_obj_t self_in, mp_obj_t index, mp_obj_t value);

extern const mp_obj_type_t lvgl_type_obj;

typedef int32_t (*lvgl_obj_attr_int_getter_t)(lv_obj_t *obj);
typedef void (*lvgl_obj_attr_int_setter_t)(lv_obj_t *obj, int32_t);
typedef void (*lvgl_obj_attr_int_deleter_t)(lv_obj_t *obj);

inline lvgl_obj_attr_int_getter_t lvgl_obj_attr_int_const(int32_t (*fun)(const lv_obj_t *obj)) {
    return (lvgl_obj_attr_int_getter_t)fun;
}

void lvgl_obj_attr_int(lvgl_obj_handle_t *handle, qstr attr, lvgl_obj_attr_int_getter_t getter, lvgl_obj_attr_int_setter_t setter, lvgl_obj_attr_int_deleter_t deleter, mp_obj_t *dest);

void lvgl_obj_attr_str(lvgl_obj_handle_t *handle, qstr attr, char *(*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, const char *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest);

// void lvgl_obj_attr_type(lvgl_obj_handle_t *handle, qstr attr, lv_type_code_t type_code, void (*getter)(const lv_obj_t *obj, void *value), void (*setter)(lv_obj_t *obj, const void *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest, void *scratch[2]);

void lvgl_obj_attr_obj(lvgl_obj_handle_t *handle, qstr attr, lv_obj_t *(*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, lv_obj_t *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest);

extern const mp_obj_type_t lvgl_type_obj_list;
