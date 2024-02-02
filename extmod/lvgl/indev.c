// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "./indev.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./super.h"

#include "py/runtime.h"

static void lvgl_indev_event_delete(lv_event_t *e);

lvgl_handle_indev_t *lvgl_handle_alloc_indev(lv_indev_t *indev, const mp_obj_type_t *type, void (*deinit_cb)(lv_indev_t *indev)) {
    assert(lvgl_is_locked());
    assert(!lv_indev_get_user_data(indev));

    lvgl_handle_indev_t *handle = malloc(sizeof(lvgl_handle_indev_t));
    handle->ref_count = 1;
    handle->lv_indev = indev;
    handle->mp_indev = NULL;
    handle->type = type;
    handle->deinit_cb = deinit_cb;

    lv_indev_set_user_data(indev, handle);
    lv_indev_add_event_cb(indev, lvgl_indev_event_delete, LV_EVENT_DELETE, NULL);

    return handle;
}

static lvgl_handle_indev_t *lvgl_handle_copy_indev(lvgl_handle_indev_t *handle) {
    assert(lvgl_is_locked());
    assert(handle->ref_count > 0);
    handle->ref_count++;
    return handle;
}

static void lvgl_handle_free_indev(lvgl_handle_indev_t *handle) {
    assert(lvgl_is_locked());
    assert(handle->ref_count > 0);
    if (--handle->ref_count == 0) {
        free(handle);
    }
}

// static lvgl_handle_indev_t *lvgl_indev_get_handle(lv_indev_t *indev) {
//     assert(lvgl_is_locked());
//     lvgl_handle_indev_t *handle = lv_indev_get_user_data(indev);
//     if (!handle) {
//         handle = lvgl_handle_alloc_indev(indev, &lvgl_type_indev, NULL);
//     }
//     assert(handle->lv_indev == indev);
//     return handle;
// }

lvgl_obj_indev_t *lvgl_handle_get_indev(lvgl_handle_indev_t *handle) {
    lvgl_obj_indev_t *self = handle->mp_indev;
    if (!self) {        
        self = m_new_obj_with_finaliser(lvgl_obj_indev_t);
        lvgl_lock();
        self->base.type = handle->type;
        self->handle = lvgl_handle_copy_indev(handle);
        handle->mp_indev = self;
        lvgl_unlock();
    }
    return self;
}

static lv_indev_t *lvgl_lock_indev(lvgl_obj_indev_t *self) {
    assert(lvgl_is_locked());
    assert(self->handle->mp_indev == self);
    lv_indev_t *indev = self->handle->lv_indev;
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

    lvgl_handle_indev_t *handle = lv_indev_get_user_data(indev);   
    if (handle) {
        assert(indev == handle->lv_indev);

        handle->lv_indev = NULL;
        if (handle->deinit_cb) {
            handle->deinit_cb(indev);
        }        
        lvgl_handle_free_indev(handle);

        #ifndef NDEBUG
        lv_indev_set_user_data(indev, NULL);
        #endif
    }
}

static lvgl_obj_indev_t *lvgl_indev_get(mp_obj_t self_in) {
    // return MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&lvgl_type_indev)));
    return MP_OBJ_TO_PTR(self_in);
}

STATIC mp_obj_t lvgl_indev_del(mp_obj_t self_in) {
    lvgl_obj_indev_t *self = lvgl_indev_get(self_in);

    lvgl_handle_indev_t *handle = self->handle;
    if (handle) {
        lvgl_lock();
        assert(handle->mp_indev = self);
        handle->mp_indev = NULL;
        lvgl_handle_free_indev(handle);
        lvgl_unlock();
        self->handle = NULL;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_indev_del_obj, lvgl_indev_del);

STATIC mp_obj_t lvgl_indev_delete(mp_obj_t self_in) {
    lvgl_obj_indev_t *self = lvgl_indev_get(self_in);
    lvgl_lock();
    lv_indev_t *indev = lvgl_lock_indev(self);
    lv_indev_delete(indev);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_indev_delete_obj, lvgl_indev_delete);

STATIC const mp_rom_map_elem_t lvgl_indev_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_indev_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&lvgl_indev_delete_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_indev_locals_dict, lvgl_indev_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_indev,
    MP_QSTR_InDev,
    MP_TYPE_FLAG_NONE,
    locals_dict, &lvgl_indev_locals_dict 
    );
