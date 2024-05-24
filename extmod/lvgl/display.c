// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "./display.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./super.h"

#include "py/runtime.h"

static void lvgl_display_event_delete(lv_event_t *e);

lvgl_display_handle_t *lvgl_display_alloc_handle(lv_display_t *disp, void (*deinit_cb)(lv_display_t *)) {
    assert(lvgl_is_locked());
    assert(!lv_display_get_user_data(disp));

    lvgl_display_handle_t *handle = malloc(sizeof(lvgl_display_handle_t));
    lvgl_ptr_init_handle(&handle->base, &lvgl_display_type, disp);
    handle->deinit_cb = deinit_cb;
    handle->buf[0] = NULL;
    handle->buf[1] = NULL;

    lv_display_set_user_data(disp, lvgl_ptr_copy(&handle->base));
    lv_display_add_event_cb(disp, lvgl_display_event_delete, LV_EVENT_DELETE, NULL);

    return handle;
}

lvgl_ptr_t lvgl_display_get_handle0(const void *lv_ptr) {
    assert(lvgl_is_locked());
    lv_display_t *disp = (void *)lv_ptr;
    lvgl_display_handle_t *handle = lv_display_get_user_data(disp);
    if (!handle) {
        handle = lvgl_display_alloc_handle(disp, NULL);
    }
    return handle;
}

bool lvgl_display_alloc_draw_buffers(lvgl_display_handle_t *handle, size_t buf_size) {
    assert(lvgl_is_locked());
    lv_display_t *disp = lvgl_display_to_lv(handle);
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
    lv_display_set_buffers(disp, handle->buf[0], handle->buf[1], buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    return true;
}

static lv_display_t *lvgl_lock_display(lvgl_display_handle_t *handle) {
    assert(lvgl_is_locked());
    lv_display_t *disp = lvgl_display_to_lv(handle);
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

    lvgl_display_handle_t *handle = lv_display_get_user_data(disp);   
    if (handle) {
        if (handle->deinit_cb) {
            handle->deinit_cb(disp);
        }
        free(handle->buf[0]);
        free(handle->buf[1]);  
        lvgl_ptr_reset(&handle->base);              
        lvgl_ptr_delete(&handle->base);
    }
}

static lvgl_display_handle_t *lvgl_display_get(mp_obj_t self_in) {
    return lvgl_ptr_from_mp(NULL, self_in);
}

void lvgl_display_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_display_handle_t *handle = lvgl_display_get(self_in);
    if (attr == MP_QSTR_screen) {
        lvgl_super_attr_check(attr, true, false, false, dest);
        lvgl_lock();
        lv_display_t *disp = lvgl_lock_display(handle);
        if (dest[0] != MP_OBJ_SENTINEL) {
            lv_obj_t *obj = lv_display_get_screen_active(disp);
            lvgl_handle_t *obj_handle = lvgl_obj_get_handle(obj);
            dest[0] = lvgl_unlock_ptr(&obj_handle->base);
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
        lv_display_t *disp = lvgl_lock_display(handle);
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
    lvgl_display_handle_t *handle = lvgl_display_get(self_in);
    lvgl_lock();
    lv_display_t *disp = lvgl_lock_display(handle);
    lv_display_delete(disp);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_display_delete_obj, lvgl_display_delete);

STATIC const mp_rom_map_elem_t lvgl_display_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_ptr_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&lvgl_display_delete_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_display_locals_dict, lvgl_display_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_display,
    MP_ROM_QSTR_CONST(MP_QSTR_Display),
    MP_TYPE_FLAG_NONE,
    attr, lvgl_display_attr,
    locals_dict, &lvgl_display_locals_dict 
    );
MP_REGISTER_OBJECT(lvgl_type_display);

const lvgl_ptr_type_t lvgl_display_type = {
    &lvgl_type_display, 
    NULL, 
    NULL, 
    lvgl_display_get_handle0, 
    NULL,
};

mp_obj_t lvgl_display_get_default(void) {
    lvgl_lock();
    lv_display_t *disp = lv_display_get_default();
    lvgl_display_handle_t *handle = lvgl_display_get_handle(disp);
    return lvgl_unlock_ptr(&handle->base);
}
