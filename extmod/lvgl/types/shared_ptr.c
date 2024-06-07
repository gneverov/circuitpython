// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "../modlvgl.h"
#include "../super.h"
#include "./shared_ptr.h"
#include "../types.h"
#include "py/runtime.h"


void lvgl_ptr_init_handle(lvgl_ptr_handle_t *handle, const struct lvgl_ptr_type *type, void *lv_ptr) {
    handle->type = type;
    handle->ref_count = 0;
    handle->mp_obj = MP_OBJ_NULL;
    handle->lv_ptr = lv_ptr;
    // printf("new %p %p\n", type, handle);
}

lvgl_ptr_t lvgl_ptr_copy(lvgl_ptr_handle_t *handle) {
    if (handle) {
        handle->ref_count++;
    }
    return handle;
}

void lvgl_ptr_delete(lvgl_ptr_handle_t *handle) {
    if (!handle) {
        return;
    }
    --handle->ref_count;
    if (handle->ref_count == 0) {
        if (handle->type->delete) {
            handle->type->delete(handle);
        }
        if (handle->type->attrs) {
            lvgl_attrs_free(handle->type->attrs, (void *)handle->lv_ptr);
        }
        // printf("del %p %p\n", handle->type, handle);
        free(handle);
    }
}

lvgl_ptr_t lvgl_ptr_from_mp(const lvgl_ptr_type_t *type, mp_obj_t obj_in) {
    if (!mp_obj_is_obj(obj_in) || (type && !mp_obj_is_exact_type(obj_in, MP_OBJ_FROM_PTR(type->mp_type)))) {
        mp_raise_TypeError(NULL);
    }
    lvgl_obj_ptr_t *obj = MP_OBJ_TO_PTR(obj_in);
    return obj->handle;
}

void lvgl_ptr_init_obj(lvgl_obj_ptr_t *obj, lvgl_ptr_handle_t *handle) {
    obj->handle = lvgl_ptr_copy(handle);
}

mp_obj_t lvgl_ptr_to_mp(lvgl_ptr_handle_t *handle) {
    if (!handle) {
        return mp_const_none;
    }    
    if (!handle->mp_obj) {
        if (handle->type->new_mp) {
            handle->mp_obj = handle->type->new_mp(handle);
        }
        else {
            lvgl_obj_ptr_t *obj = mp_obj_malloc_with_finaliser(lvgl_obj_ptr_t, handle->type->mp_type);
            lvgl_ptr_init_obj(obj, handle);
            handle->mp_obj = MP_OBJ_FROM_PTR(obj);
        }
    }
    return handle->mp_obj;
}

lvgl_ptr_t lvgl_ptr_from_lv(const lvgl_ptr_type_t *type, const void* lv_ptr) {
    if (!lv_ptr) {
        return NULL;
    }
    assert(type->get_handle);
    lvgl_ptr_handle_t *handle = type->get_handle(lv_ptr);
    return handle;
}

void lvgl_ptr_reset(lvgl_ptr_handle_t *handle) {
    if (handle) {
        handle->lv_ptr = NULL;
    }
}

mp_obj_t lvgl_unlock_ptr(lvgl_ptr_handle_t *handle) {
    lvgl_ptr_copy(handle);
    lvgl_unlock();
    mp_obj_t obj = lvgl_ptr_to_mp(handle);
    lvgl_ptr_delete(handle);
    return obj;
}

mp_obj_t lvgl_ptr_del(mp_obj_t self_in) {
    lvgl_obj_ptr_t *self = MP_OBJ_TO_PTR(self_in);
    lvgl_ptr_handle_t *handle = self->handle;
    assert(handle->mp_obj == MP_OBJ_TO_PTR(self));
    handle->mp_obj = NULL;
    lvgl_ptr_delete(handle);
    self->handle = NULL;
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(lvgl_ptr_del_obj, lvgl_ptr_del);

void lvgl_ptr_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_ptr_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    if (!lvgl_attrs_attr(attr, dest, handle->type->attrs, handle->lv_ptr)) {
        dest[1] = MP_OBJ_SENTINEL;
    }
}
