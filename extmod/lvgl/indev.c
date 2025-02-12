// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "./indev.h"
#include "./misc.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./super.h"

#include "py/runtime.h"

static void lvgl_indev_event_delete(lv_event_t *e);

lvgl_indev_handle_t *lvgl_indev_alloc_handle(lv_indev_t *indev, void (*deinit_cb)(lv_indev_t *)) {
    assert(lvgl_is_locked());
    assert(!lv_indev_get_user_data(indev));

    lvgl_indev_handle_t *handle = malloc(sizeof(lvgl_indev_handle_t));
    lvgl_ptr_init_handle(&handle->base, &lvgl_indev_type, indev);
    handle->deinit_cb = deinit_cb;

    lv_indev_set_user_data(indev, lvgl_ptr_copy(&handle->base));
    lv_indev_add_event_cb(indev, lvgl_indev_event_delete, LV_EVENT_DELETE, NULL);

    return handle;
}

static lvgl_ptr_t lvgl_indev_get_handle(const void *lv_ptr) {
    assert(lvgl_is_locked());    
    lv_indev_t *indev = (void *)lv_ptr;
    lvgl_indev_handle_t *handle = lv_indev_get_user_data(indev);
    if (!handle) {
        handle = lvgl_indev_alloc_handle(indev, NULL);
    }
    return handle;
}

static lv_indev_t *lvgl_lock_indev(lvgl_indev_handle_t *handle) {
    assert(lvgl_is_locked());
    lv_indev_t *indev = lvgl_ptr_to_lv(&handle->base);
    if (!indev) {
        lvgl_unlock();
        mp_raise_ValueError(MP_ERROR_TEXT("invalid lvgl indev"));
    }
    return indev;
}

static void lvgl_indev_event_delete(lv_event_t *e) {
    assert(e->code == LV_EVENT_DELETE);
    assert(lvgl_is_locked());
    lv_indev_t *indev = e->current_target;

    lvgl_indev_handle_t *handle = lv_indev_get_user_data(indev);   
    if (handle) {
        if (handle->deinit_cb) {
            handle->deinit_cb(indev);
        }
        lvgl_ptr_reset(&handle->base);        
        lvgl_ptr_delete(&handle->base);
    }
}

static mp_obj_t lvgl_indev_delete(mp_obj_t self_in) {
    lvgl_indev_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    lvgl_lock();
    lv_indev_t *indev = lvgl_lock_indev(handle);
    lv_indev_delete(indev);
    lvgl_unlock();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(lvgl_indev_delete_obj, lvgl_indev_delete);

static mp_obj_t lvgl_indev_get_vect(mp_obj_t self_in) {
    lvgl_indev_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    lvgl_lock();
    lv_indev_t *indev = lvgl_lock_indev(handle);
    lv_point_t point;
    lv_indev_get_vect(indev, &point);
    lvgl_unlock();

    return lvgl_point_to_mp(&point);
}
static MP_DEFINE_CONST_FUN_OBJ_1(lvgl_indev_get_vect_obj, lvgl_indev_get_vect);

static const mp_rom_map_elem_t lvgl_indev_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_ptr_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&lvgl_indev_delete_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_vect),        MP_ROM_PTR(&lvgl_indev_get_vect_obj) },
};
static MP_DEFINE_CONST_DICT(lvgl_indev_locals_dict, lvgl_indev_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_indev,
    MP_ROM_QSTR_CONST(MP_QSTR_InDev),
    MP_TYPE_FLAG_NONE,
    locals_dict, &lvgl_indev_locals_dict 
    );
MP_REGISTER_OBJECT(lvgl_type_indev);

const lvgl_ptr_type_t lvgl_indev_type = {
    &lvgl_type_indev, 
    NULL, 
    NULL, 
    lvgl_indev_get_handle, 
    NULL,
};

mp_obj_t lvgl_indev_list(void) {
    mp_obj_t list = mp_obj_new_list(0, NULL);
    lvgl_lock();
    lv_indev_t *indev = lv_indev_get_next(NULL);
    lvgl_indev_handle_t *handle = lvgl_ptr_copy(lvgl_ptr_from_lv(&lvgl_indev_type, indev));
    lvgl_unlock();

    while (handle) {
        mp_obj_t elem = lvgl_ptr_to_mp(&handle->base);
        mp_obj_list_append(list, elem);

        lvgl_lock();
        lv_indev_t *indev = lvgl_ptr_to_lv(&handle->base);
        lvgl_indev_handle_t *tmp = NULL;
        if (indev) {
            indev = lv_indev_get_next(indev);
            tmp = lvgl_ptr_copy(lvgl_ptr_from_lv(&lvgl_indev_type, indev));
        }
        lvgl_unlock();

        lvgl_ptr_delete(&handle->base);
        handle = tmp;
    }
    return list;
}
