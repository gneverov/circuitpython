// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./anim.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./super.h"
#include "./types.h"

#include "extmod/freeze/extmod.h"
#include "py/runtime.h"


lvgl_ptr_t lvgl_anim_get_handle(const void *lv_ptr) {
    const lv_anim_t *anim = lv_ptr;
    return lv_anim_get_user_data(anim);
}

static void lvgl_anim_del_event(void *arg) {
    lvgl_anim_event_t *event = arg;
    lvgl_ptr_delete(&event->handle->base);
    free(event);
}

static void lvgl_anim_run_event(void *arg) {
    lvgl_anim_event_t *event = arg;
    mp_obj_t func = gc_handle_get(event->handle->mp_exec_cb);
    if (func == MP_OBJ_NULL) {
        return;
    }
    if (!lvgl_ptr_to_lv(&event->handle->base)) {
        return;
    }
    mp_obj_t anim = lvgl_ptr_to_mp(&event->handle->base);
    mp_call_function_2(func, anim, MP_OBJ_NEW_SMALL_INT(event->value));
}

STATIC void lvgl_anim_custom_exec_cb(lv_anim_t *anim, int32_t value) {
    lvgl_anim_handle_t *handle = lvgl_anim_get_handle(anim);
    if (!handle) {
        return;
    }

    lv_style_prop_t *prop = handle->props;
    while (*prop) {
        lv_style_value_t svalue = { .num = value };
        lv_obj_set_local_style_prop(anim->var, *prop, svalue, 0);
        prop++;
    }
   
    if (!handle->mp_exec_cb) {
        return;
    }

    lvgl_queue_t *queue = lvgl_queue_default;
    if (!queue) {
        return;
    }

    lvgl_anim_event_t *event = malloc(sizeof(lvgl_anim_event_t));
    event->elem.run = lvgl_anim_run_event;
    event->elem.del = lvgl_anim_del_event;
    event->handle = lvgl_ptr_copy(&handle->base);
    event->value = value;
    lvgl_queue_send(queue, &event->elem);
}

STATIC void lvgl_anim_deleted_cb(lv_anim_t *anim) {
    lvgl_anim_handle_t *handle = lvgl_anim_get_handle(anim);
    if (!handle) {
        return;
    }
    lvgl_ptr_delete(&handle->base);
}

STATIC mp_obj_t lvgl_anim_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, true);
    lvgl_anim_handle_t *other = MP_OBJ_NULL;
    if (n_args > 0) {
        other = lvgl_ptr_from_mp(&lvgl_anim_type, args[0]);
    }

    lvgl_anim_handle_t *handle = lv_malloc_zeroed(sizeof(lvgl_anim_handle_t));
    lvgl_ptr_init_handle(&handle->base, &lvgl_anim_type, &handle->anim);
    if (other) {
        handle->anim = other->anim;
        lvgl_lock();
        lvgl_type_clone(LV_TYPE_OBJ_HANDLE, &handle->var, &other->var);
        lvgl_unlock();
        lvgl_type_clone(LV_TYPE_PROP_LIST, &handle->props, &other->props);
        lvgl_type_clone(LV_TYPE_GC_HANDLE, &handle->mp_exec_cb, &other->mp_exec_cb);
    }
    else {
        lv_anim_init(&handle->anim);
        lv_anim_set_custom_exec_cb(&handle->anim, lvgl_anim_custom_exec_cb);
        lv_anim_set_deleted_cb(&handle->anim, lvgl_anim_deleted_cb);
    }
    lv_anim_set_user_data(&handle->anim, handle);
    mp_obj_t self_out = lvgl_ptr_to_mp(&handle->base);

    lvgl_super_update(self_out, n_kw, (mp_map_elem_t *)(args + n_args));
    return self_out;
}

STATIC void lvgl_anim_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_anim_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);

    if (attr == MP_QSTR_var) {
        lvgl_type_attr(attr, dest, LV_TYPE_OBJ_HANDLE, &handle->var);
    }
    else if (attr == MP_QSTR_props) {
        lvgl_type_attr(attr, dest, LV_TYPE_PROP_LIST, &handle->props);
    }
    else if (attr == MP_QSTR_exec_cb) {
        lvgl_type_attr(attr, dest, LV_TYPE_GC_HANDLE, &handle->mp_exec_cb);
    }
    else if (attr == MP_QSTR_delay) {
        uint32_t delay = lv_anim_get_delay(&handle->anim);
        delay = lvgl_bitfield_attr_int(attr, dest, delay);
        lv_anim_set_delay(&handle->anim, delay);
    }
    else {
        lvgl_ptr_attr(self_in, attr, dest);
    }
}

STATIC mp_obj_t lvgl_anim_start(mp_obj_t self_in) {
    lvgl_anim_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    lvgl_lock();
    lv_obj_t *var = lvgl_lock_obj(handle->var);
    lv_anim_set_var(&handle->anim, var);
    lv_anim_start(&handle->anim);
    lvgl_ptr_copy(&handle->base);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_anim_start_obj, lvgl_anim_start);

STATIC mp_obj_t lvgl_anim_delete(mp_obj_t self_in) {
    lvgl_anim_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    lvgl_lock();
    lv_obj_t *var = lvgl_lock_obj(handle->var);
    bool ret = lv_anim_delete(var, NULL);
    lvgl_unlock();
    return mp_obj_new_bool(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_anim_delete_obj, lvgl_anim_delete);

STATIC const mp_rom_map_elem_t lvgl_anim_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_ptr_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_start),           MP_ROM_PTR(&lvgl_anim_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&lvgl_anim_delete_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_anim_locals_dict, lvgl_anim_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_anim,
    MP_ROM_QSTR_CONST(MP_QSTR_Anim),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_anim_make_new,
    attr, lvgl_anim_attr,
    locals_dict, &lvgl_anim_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_anim);

STATIC const lvgl_type_attr_t lvgl_anim_attrs[] = {
    { MP_ROM_QSTR_CONST(MP_QSTR_path_cb), offsetof(lv_anim_t, path_cb), LV_TYPE_ANIM_PATH },
    { MP_ROM_QSTR_CONST(MP_QSTR_start_value), offsetof(lv_anim_t, start_value), LV_TYPE_INT32 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_current_value), offsetof(lv_anim_t, current_value), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_end_value), offsetof(lv_anim_t, end_value), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_duration), offsetof(lv_anim_t, duration), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_playback_duration), offsetof(lv_anim_t, playback_duration), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_playback_delay), offsetof(lv_anim_t, playback_delay), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_repeat_count), offsetof(lv_anim_t, repeat_cnt), LV_TYPE_INT16 },
    { MP_ROM_QSTR_CONST(MP_QSTR_repeat_delay), offsetof(lv_anim_t, repeat_delay), LV_TYPE_INT32 },
    { 0 },
};
MP_REGISTER_STRUCT(lvgl_anim_attrs, lvgl_type_attr_t);

static void lvgl_anim_deinit(lvgl_ptr_t ptr) {
    lvgl_anim_handle_t *handle = ptr;
    lvgl_type_free(LV_TYPE_OBJ_HANDLE, &handle->var);
    lvgl_type_free(LV_TYPE_PROP_LIST, &handle->props);
    lvgl_type_free(LV_TYPE_GC_HANDLE, &handle->mp_exec_cb);
}

const lvgl_ptr_type_t lvgl_anim_type = {
    &lvgl_type_anim,
    NULL,
    lvgl_anim_deinit,
    lvgl_anim_get_handle,
    lvgl_anim_attrs,
};


typedef lvgl_obj_static_ptr_t lvgl_obj_anim_path_t;

STATIC const lvgl_obj_anim_path_t lvgl_anim_paths[] = {
    { { &lvgl_type_anim_path }, lv_anim_path_linear },
    { { &lvgl_type_anim_path }, lv_anim_path_ease_in },
    { { &lvgl_type_anim_path }, lv_anim_path_ease_out },
    { { &lvgl_type_anim_path }, lv_anim_path_ease_in_out },
    { { &lvgl_type_anim_path }, lv_anim_path_overshoot },
    { { &lvgl_type_anim_path }, lv_anim_path_bounce },
    { { &lvgl_type_anim_path }, lv_anim_path_step },
    // { { &lvgl_type_anim_path }, lv_anim_path_custom_bezier3 },
};

STATIC mp_obj_t lvgl_anim_path_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 2, 2, false);
    lvgl_obj_anim_path_t *self = MP_OBJ_TO_PTR(args[0]);
    lv_anim_path_cb_t anim_path = self->lv_ptr;
    lv_anim_t *anim = lvgl_ptr_from_mp(&lvgl_anim_type, args[1]);
    int32_t value = anim_path(anim);
    return mp_obj_new_int(value);
}

STATIC const mp_rom_map_elem_t lvgl_anim_path_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_LINEAR),          MP_ROM_PTR(&lvgl_anim_paths[0]) },
    { MP_ROM_QSTR(MP_QSTR_EASE_IN),         MP_ROM_PTR(&lvgl_anim_paths[1]) },
    { MP_ROM_QSTR(MP_QSTR_EASE_OUT),        MP_ROM_PTR(&lvgl_anim_paths[2]) },
    { MP_ROM_QSTR(MP_QSTR_EASE_IN_OUT),     MP_ROM_PTR(&lvgl_anim_paths[3]) },
    { MP_ROM_QSTR(MP_QSTR_OVERSHOOT),       MP_ROM_PTR(&lvgl_anim_paths[4]) },
    { MP_ROM_QSTR(MP_QSTR_BOUNCE),          MP_ROM_PTR(&lvgl_anim_paths[5]) },
    { MP_ROM_QSTR(MP_QSTR_STEP),            MP_ROM_PTR(&lvgl_anim_paths[6]) },
    { MP_ROM_QSTR(MP_QSTR_CUSTOM_BEZIER3),  MP_ROM_PTR(&lvgl_anim_paths[7]) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_anim_path_locals_dict, lvgl_anim_path_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_anim_path,
    MP_ROM_QSTR_CONST(MP_QSTR_AnimPath),
    MP_TYPE_FLAG_NONE,
    call, &lvgl_anim_path_call,
    locals_dict, &lvgl_anim_path_locals_dict 
    );
MP_REGISTER_OBJECT(lvgl_type_anim_path);

const lvgl_static_ptr_type_t lvgl_anim_path_type = {
    &lvgl_type_anim_path,
    &lvgl_anim_path_locals_dict.map,
};
