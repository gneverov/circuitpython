// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "./display.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./super.h"

#include "py/runtime.h"

static void lvgl_display_event_delete(lv_event_t *e);

lvgl_handle_display_t *lvgl_handle_alloc_display(lv_display_t *disp, const mp_obj_type_t *type, void (*deinit_cb)(lv_display_t *disp)) {
    assert(lvgl_is_locked());
    assert(!lv_display_get_user_data(disp));

    lvgl_handle_display_t *handle = malloc(sizeof(lvgl_handle_display_t));
    handle->ref_count = 1;
    handle->lv_disp = disp;
    handle->mp_disp = NULL;
    handle->type = type;
    handle->deinit_cb = deinit_cb;
    handle->buf[0] = NULL;
    handle->buf[1] = NULL;

    lv_display_set_user_data(disp, handle);
    lv_display_add_event_cb(disp, lvgl_display_event_delete, LV_EVENT_DELETE, NULL);

    return handle;
}

static lvgl_handle_display_t *lvgl_handle_copy_display(lvgl_handle_display_t *handle) {
    assert(lvgl_is_locked());
    assert(handle->ref_count > 0);
    handle->ref_count++;
    return handle;
}

static void lvgl_handle_free_display(lvgl_handle_display_t *handle) {
    assert(lvgl_is_locked());
    assert(handle->ref_count > 0);
    if (--handle->ref_count == 0) {
        free(handle);
    }
}

static lvgl_handle_display_t *lvgl_display_get_handle(lv_display_t *disp) {
    assert(lvgl_is_locked());
    if (!disp) {
        return NULL;
    }
    lvgl_handle_display_t *handle = lv_display_get_user_data(disp);
    if (!handle) {
        handle = lvgl_handle_alloc_display(disp, &lvgl_type_display, NULL);
    }
    assert(handle->lv_disp == disp);
    return handle;
}

lvgl_obj_display_t *lvgl_handle_get_display(lvgl_handle_display_t *handle) {
    if (!handle) {
        return NULL;
    }
    lvgl_obj_display_t *self = handle->mp_disp;
    if (!self) {
        self = m_new_obj_with_finaliser(lvgl_obj_display_t);
        lvgl_lock();
        self->base.type = handle->type;
        self->handle = lvgl_handle_copy_display(handle);
        handle->mp_disp = self;
        lvgl_unlock();
    }
    return self;
}

bool lvgl_handle_alloc_display_draw_buffers(lvgl_handle_display_t *handle, size_t buf_size) {
    assert(lvgl_is_locked());
    lv_display_t *disp = handle->lv_disp;
    assert(disp);
    assert(!handle->buf[0] && !handle->buf[0]);
    if (!buf_size) {
        buf_size = 
            lv_display_get_horizontal_resolution(disp) *
            lv_display_get_vertical_resolution(disp) * 
            lv_color_format_get_size(lv_display_get_color_format(disp)) /
            10;
    }

    handle->buf[0] = malloc(buf_size);
    handle->buf[1] = malloc(buf_size);
    if (!handle->buf[0] || !handle->buf[1]) {
        return false;        
    }
    lv_display_set_draw_buffers(disp, handle->buf[0], handle->buf[1], buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    return true;
}

static lv_display_t *lvgl_lock_display(lvgl_obj_display_t *self) {
    assert(lvgl_is_locked());
    assert(self->handle->mp_disp == self);
    lv_display_t *disp = self->handle->lv_disp;
    if (!disp) {
        lvgl_unlock();
        mp_raise_ValueError(MP_ERROR_TEXT("invalid lvgl display"));
    }
    return disp;
}

static void lvgl_display_event_delete(lv_event_t *e) {
    assert(e->code == LV_EVENT_DELETE);
    assert(lvgl_is_locked());
    lv_display_t *disp = e->current_target;

    lvgl_handle_display_t *handle = lv_display_get_user_data(disp);   
    if (handle) {
        assert(disp == handle->lv_disp);

        handle->lv_disp = NULL;
        if (handle->deinit_cb) {
            handle->deinit_cb(disp);
        }
        free(handle->buf[0]);
        free(handle->buf[1]);        
        lvgl_handle_free_display(handle);

        #ifndef NDEBUG
        lv_display_set_user_data(disp, NULL);
        #endif
    }
}

static lvgl_obj_display_t *lvgl_display_get(mp_obj_t self_in) {
    // return MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&lvgl_type_display)));
    return MP_OBJ_TO_PTR(self_in);
}

STATIC mp_obj_t lvgl_display_del(mp_obj_t self_in) {
    lvgl_obj_display_t *self = lvgl_display_get(self_in);

    lvgl_handle_display_t *handle = self->handle;
    if (handle) {
        lvgl_lock();
        assert(handle->mp_disp = self);
        handle->mp_disp = NULL;
        lvgl_handle_free_display(handle);
        lvgl_unlock();
        self->handle = NULL;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_display_del_obj, lvgl_display_del);

void lvgl_display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_display_t *self = lvgl_display_get(self_in);
    if (attr == MP_QSTR_screen) {
        lvgl_super_attr_check(attr, true, false, false, dest);
        lvgl_lock();
        lv_display_t *disp = lvgl_lock_display(self);
        if (dest[0] != MP_OBJ_SENTINEL) {
            lv_obj_t *scr = lv_display_get_screen_active(disp);
            lvgl_handle_t *handle = lvgl_obj_get_handle(scr);
            lvgl_unlock();
            dest[0] = MP_OBJ_FROM_PTR(lvgl_handle_get_obj(handle));
            return;
        }
        lvgl_unlock();
    }
    else if (attr == MP_QSTR_rotation) {
        lvgl_super_attr_check(attr, true, true, false, dest);
        lv_disp_rotation_t rot = 0;
        if (dest[1] != MP_OBJ_NULL) {
            mp_int_t value = mp_obj_get_int(dest[1]);
            switch (value) {
                case 0:
                    rot = LV_DISP_ROTATION_0;
                    break;
                case 90:
                    rot = LV_DISP_ROTATION_90;
                    break;
                case 180:
                    rot = LV_DISP_ROTATION_180;
                    break;
                case 270:
                    rot = LV_DISP_ROTATION_270;
                    break;
                default:
                    mp_raise_ValueError(NULL);              
            }
        }

        lvgl_lock();
        lv_display_t *disp = lvgl_lock_display(self);
        if (dest[0] != MP_OBJ_SENTINEL) {
            rot = lv_display_get_rotation(disp);
            switch (rot) {
                case LV_DISP_ROTATION_0:
                    dest[0] = MP_OBJ_NEW_SMALL_INT(0);
                    break;
                case LV_DISP_ROTATION_90:
                    dest[0] = MP_OBJ_NEW_SMALL_INT(90);
                    break;
                case LV_DISP_ROTATION_180:
                    dest[0] = MP_OBJ_NEW_SMALL_INT(180);
                    break;
                case LV_DISP_ROTATION_270:
                    dest[0] = MP_OBJ_NEW_SMALL_INT(270);
                    break;
                default:
                    dest[0] = mp_const_none;
                    break;
            }
        }
        else if (dest[1] != MP_OBJ_NULL) {
            lv_display_set_rotation(disp, rot);
            dest[0] = MP_OBJ_NULL;
        }
        lvgl_unlock();
    }
    else {
        lvgl_super_attr(self_in, &lvgl_type_display, attr, dest);
    }
}

STATIC mp_obj_t lvgl_display_delete(mp_obj_t self_in) {
    lvgl_obj_display_t *self = lvgl_display_get(self_in);
    lvgl_lock();
    lv_display_t *disp = lvgl_lock_display(self);
    lv_display_delete(disp);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_display_delete_obj, lvgl_display_delete);

STATIC const mp_rom_map_elem_t lvgl_display_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_display_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&lvgl_display_delete_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_display_locals_dict, lvgl_display_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_display,
    MP_QSTR_Display,
    MP_TYPE_FLAG_NONE,
    attr, lvgl_display_attr,
    locals_dict, &lvgl_display_locals_dict 
    );

mp_obj_t lvgl_display_get_default(void) {
    lvgl_lock();
    lv_display_t *disp = lv_display_get_default();
    lvgl_handle_display_t *handle = lvgl_display_get_handle(disp);
    lvgl_unlock();
    lvgl_obj_display_t *self = lvgl_handle_get_display(handle);
    return self ? MP_OBJ_FROM_PTR(self) : mp_const_none;
}
