// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "./modlvgl.h"
#include "./obj.h"
#include "./obj_class.h"
#include "./style.h"
#include "./super.h"

#include "py/gc_handle.h"
#include "py/objstr.h"
#include "py/runtime.h"


static mp_obj_t lvgl_obj_delete(mp_obj_t self_in);
static void lvgl_obj_attr_style_prop(lvgl_handle_t *handle, lv_style_prop_t prop, mp_obj_t *dest, lv_style_selector_t selector, lv_type_code_t type);
static void lvgl_obj_event_delete(lv_event_t *e);
static void lvgl_obj_event_cb(lv_event_t *e);

lvgl_handle_t *lvgl_handle_alloc(lv_obj_t *obj) {
    assert(lvgl_is_locked());
    assert(!lv_obj_get_user_data(obj));

    lvgl_handle_t *handle = malloc(sizeof(lvgl_handle_t));
    lvgl_ptr_init_handle(&handle->base, &lvgl_obj_type, obj);

    lv_obj_set_user_data(obj, lvgl_ptr_copy(&handle->base));
    lv_obj_add_event_cb(obj, lvgl_obj_event_delete, LV_EVENT_DELETE, NULL);

    return handle;
}

lvgl_ptr_t lvgl_obj_get_handle0(const void *lv_ptr) {
    assert(lvgl_is_locked());
    lv_obj_t *obj = (void *)lv_ptr;
    lvgl_handle_t *handle = lv_obj_get_user_data(obj);
    if (!handle) {
        handle = lvgl_handle_alloc(obj);
    }
    return handle;
}

mp_obj_t lvgl_obj_new(lvgl_ptr_t ptr) {
    lvgl_handle_t *handle = ptr;
    const mp_obj_type_t *mp_type = lvgl_obj_type.mp_type;
    lvgl_lock();
    const lv_obj_t *obj = lvgl_obj_to_lv(handle);
    if (obj) {
        const lv_obj_class_t *lv_class = lv_obj_get_class(obj);
        mp_type = lvgl_class_lookup(lv_class)->mp_type;
    }
    lvgl_unlock();
    
    lvgl_obj_t *self = m_new_obj_with_finaliser(lvgl_obj_t);
    lvgl_ptr_init_obj(&self->part.base, mp_type, &handle->base);
    self->part.selector = 0;
    self->part.whole = self;
    mp_map_init(&self->members, 0);
    self->children.type = &lvgl_type_obj_list;    
    return MP_OBJ_FROM_PTR(self);
}

static int lvgl_obj_preremove_style(lv_obj_t *obj, const lv_style_t *style, lv_style_selector_t selector) {
    assert(lvgl_is_locked());
    lv_state_t state = lv_obj_style_get_selector_state(selector);
    lv_part_t part = lv_obj_style_get_selector_part(selector);

    int ref_count = 0;
    for (uint32_t i = 0; i < obj->style_cnt; i++) {
        lv_state_t state_act = lv_obj_style_get_selector_state(obj->styles[i].selector);
        lv_part_t part_act = lv_obj_style_get_selector_part(obj->styles[i].selector);
        if((state != LV_STATE_ANY && state_act != state) ||
            (part != LV_PART_ANY && part_act != part) ||
            (style != NULL && style != obj->styles[i].style)) {
            continue;
        }

        const lv_style_t *obj_style = obj->styles[i].style;
        lvgl_style_handle_t *handle = lvgl_style_get_handle(obj_style);
        // TODO: don't delete the style under lvgl lock
        lvgl_ptr_delete(&handle->base);
        ref_count++;
    }
    return ref_count;
}

static void lvgl_obj_event_delete(lv_event_t *e) {
    assert(e->code == LV_EVENT_DELETE);
    assert(lvgl_is_locked());
    lv_obj_t *obj = e->current_target;

    uint32_t count = lv_obj_get_event_count(obj);
    for (uint32_t i = 0; i < count; i++) {
        lv_event_dsc_t *dsc = lv_obj_get_event_dsc(obj, i);
        if (lv_event_dsc_get_cb(dsc) == lvgl_obj_event_cb) {
            // a subsequent MP callbacks cannot be called since the obj handle is freed by this function
            dsc->cb = NULL;
            gc_handle_t *user_data = lv_event_dsc_get_user_data(dsc);
            gc_handle_free(user_data);
        }
    }

    lvgl_obj_preremove_style(obj, NULL, LV_PART_ANY | LV_STATE_ANY);

    lvgl_handle_t *handle = lv_obj_get_user_data(obj);
    if (handle) {
        lvgl_ptr_reset(&handle->base);
        lvgl_ptr_delete(&handle->base);
    }
}

lvgl_obj_t *lvgl_obj_get_whole(mp_obj_t self) {
    lvgl_obj_part_t *part = MP_OBJ_TO_PTR(self);
    return part->whole;
}

lvgl_handle_t *lvgl_obj_get(mp_obj_t self, lv_style_selector_t *selector) {
    lvgl_obj_part_t *part = MP_OBJ_TO_PTR(self);
    if (selector) {
        *selector = part->selector;
    }
    return lvgl_ptr_from_mp(NULL, MP_OBJ_FROM_PTR(part->whole));
}

lvgl_handle_t *lvgl_obj_get_checked(mp_obj_t self) {
    const mp_obj_type_t *type = mp_obj_get_type(self);
    if (!mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(type), MP_OBJ_FROM_PTR(&lvgl_type_obj))) {
        mp_raise_msg_varg(&mp_type_TypeError, MP_ERROR_TEXT("'%q' object isn't an lvgl object"), (qstr)type->name);
    }    
    return lvgl_obj_get(self, NULL);
}

lv_obj_t *lvgl_lock_obj(lvgl_handle_t *handle) {
    assert(lvgl_is_locked());
    lv_obj_t *obj = lvgl_obj_to_lv(handle);
    if (!obj) {
        lvgl_unlock();
        mp_raise_ValueError(MP_ERROR_TEXT("invalid lvgl object"));
    }
    return obj;    
}

mp_obj_t lvgl_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 1, true);

    assert(mp_obj_is_subclass_fast(type, MP_OBJ_FROM_PTR(&lvgl_type_obj)));
    assert(MP_OBJ_TYPE_HAS_SLOT(type, protocol));
    const lv_obj_class_t *lv_class = MP_OBJ_TYPE_GET_SLOT(type, protocol);

    mp_obj_t parent_in = n_args > 0 ? args[0] : MP_OBJ_NULL;
    lvgl_handle_t *parent_handle = NULL;
    if ((parent_in != MP_OBJ_NULL) && (parent_in != mp_const_none)) {
        parent_handle = lvgl_obj_get_checked(parent_in);
    }

    lvgl_lock_init();
    lv_obj_t *parent_obj = NULL;
    if (parent_handle) {
        parent_obj = lvgl_lock_obj(parent_handle);
    }
    else if (!parent_in) {
        parent_obj = lv_screen_active();
        if (!parent_obj) {
            lvgl_unlock();
            mp_raise_ValueError(MP_ERROR_TEXT("no display"));
        }        
    }
    lv_obj_t *obj = lv_obj_class_create_obj(lv_class, parent_obj);
    if (!obj) {
        lvgl_unlock();
        mp_raise_ValueError(MP_ERROR_TEXT("no display"));
    }     
    lv_obj_class_init_obj(obj);
    lvgl_handle_t *handle = lvgl_obj_get_handle(obj);
    mp_obj_t self_out = lvgl_unlock_ptr(&handle->base);

    lvgl_obj_t *self = MP_OBJ_TO_PTR(self_out);
    self->members.is_fixed = 1;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        lvgl_super_update(MP_OBJ_FROM_PTR(self), n_kw, (mp_map_elem_t *)(args + n_args));
        nlr_pop();
    }
    else {
        lvgl_obj_delete(self_out);
        nlr_raise(nlr.ret_val);
    }

    self->members.is_fixed = 0;
    return MP_OBJ_FROM_PTR(self);
}

static int32_t lv_obj_get_flags(const lv_obj_t *obj) {
    return obj->flags;
}

static int32_t lv_obj_get_state_0(const lv_obj_t *obj) {
    return lv_obj_get_state(obj);
}

void lvgl_obj_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lv_style_selector_t selector;
    lvgl_handle_t *handle = lvgl_obj_get(self_in, &selector);
    lvgl_obj_t *self = lvgl_obj_get_whole(self_in);
    if (attr == MP_QSTR_children) {
        lvgl_super_attr_check(attr, true, false, false, dest);
        dest[0] = MP_OBJ_FROM_PTR(&self->children);
        return;
    }
    else if (attr == MP_QSTR_index) {
        lvgl_obj_attr_int(handle, attr, lv_obj_get_index, NULL, NULL, dest);
        return;
    }
    else if (attr == MP_QSTR_parent) {
        lvgl_obj_attr_obj(handle, attr, lv_obj_get_parent, lv_obj_set_parent, NULL, dest);
        return;
    }
    else if (attr == MP_QSTR_flags) {
        lvgl_obj_attr_int(handle, attr, lv_obj_get_flags, NULL, NULL, dest);
        return;
    }
    else if (attr == MP_QSTR_state) {
        lvgl_obj_attr_int(handle, attr, lv_obj_get_state_0, NULL, NULL, dest);
        return;
    }

    lv_type_code_t type;
    lv_style_prop_t prop = lvgl_style_lookup(attr, &type);
    if (prop) {
        lvgl_obj_attr_style_prop(handle, prop, dest, selector, type);    
        return;
    }

    mp_obj_t key = MP_OBJ_NEW_QSTR(attr);
    mp_map_elem_t *elem = mp_map_lookup(&self->members, key, MP_MAP_LOOKUP);
    if (elem) {
        if (dest[0] != MP_OBJ_SENTINEL) {
            dest[0] = elem->value;
        }
        else if (dest[1] != MP_OBJ_NULL) {
            elem->value = dest[1];
            dest[0] = MP_OBJ_NULL;
        }
        else {
            mp_map_lookup(&self->members, key, MP_MAP_LOOKUP_REMOVE_IF_FOUND); 
            dest[0] = MP_OBJ_NULL;            
        }
        return;
    }

    lvgl_super_attr(self_in, &lvgl_type_obj, attr, dest);
    if (dest[0] != MP_OBJ_SENTINEL) {
        return;
    }

    if ((dest[1] != MP_OBJ_NULL) && !self->members.is_fixed) {
        elem = mp_map_lookup(&self->members, key, MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
        assert(elem);
        elem->value = dest[1];
        dest[0] = MP_OBJ_NULL;
    }
}

STATIC mp_obj_t lvgl_obj_subscr(mp_obj_t self_in, mp_obj_t index, mp_obj_t value) {
    lvgl_obj_t *self = lvgl_obj_get_whole(self_in);
    lv_style_selector_t selector = mp_obj_get_int(index);
    lvgl_super_subscr_check(mp_obj_get_type(self_in), true, false, false, value);
    if (selector == 0) {
        return MP_OBJ_FROM_PTR(self);
    }

    lvgl_obj_part_t *part = m_new_obj(lvgl_obj_part_t);
    part->base = self->part.base;
    part->selector = selector;
    part->whole = self;
    return MP_OBJ_FROM_PTR(part);
}

STATIC mp_obj_t lvgl_obj_delete(mp_obj_t self_in) {
    lvgl_handle_t *handle = lvgl_obj_get(self_in, NULL);
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_obj_delete(obj);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_obj_delete_obj, lvgl_obj_delete);

STATIC mp_obj_t lvgl_obj_update(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    lvgl_super_update(args[0], kws->alloc, kws->table);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(lvgl_obj_update_obj, 1, lvgl_obj_update);

STATIC mp_obj_t lvgl_obj_update_flag(mp_obj_t self_in, mp_obj_t flag_in, mp_obj_t value_in) {
    lvgl_handle_t *handle = lvgl_obj_get(self_in, NULL);
    mp_int_t flag = mp_obj_get_int(flag_in);
    bool value = mp_obj_is_true(value_in);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_obj_update_flag(obj, flag, value);
    lvgl_unlock();
    return mp_const_none;    
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lvgl_obj_update_flag_obj, lvgl_obj_update_flag);

STATIC mp_obj_t lvgl_obj_update_state(mp_obj_t self_in, mp_obj_t state_in, mp_obj_t value_in) {
    lvgl_handle_t *handle = lvgl_obj_get(self_in, NULL);
    mp_int_t state = mp_obj_get_int(state_in);
    bool value = mp_obj_is_true(value_in);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_obj_set_state(obj, state, value);
    lvgl_unlock();
    return mp_const_none;    
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lvgl_obj_update_state_obj, lvgl_obj_update_state);

STATIC mp_obj_t lvgl_obj_add_event(mp_obj_t self_in, mp_obj_t event_cb, mp_obj_t filter_in) {
    lvgl_handle_t *handle = lvgl_obj_get(self_in, NULL);
    if (!mp_obj_is_callable(event_cb)) {
        mp_raise_TypeError(NULL);
    }
    mp_int_t filter = mp_obj_get_int(filter_in);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    gc_handle_t *user_data = gc_handle_alloc(MP_OBJ_TO_PTR(event_cb));
    lv_obj_add_event_cb(obj, lvgl_obj_event_cb, filter, user_data);
    lvgl_unlock();
    return mp_const_none;
};
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lvgl_obj_add_event_obj, lvgl_obj_add_event);

STATIC mp_obj_t lvgl_obj_remove_event(mp_obj_t self_in, mp_obj_t event_cb) {
    lvgl_handle_t *handle = lvgl_obj_get(self_in, NULL);
    if (!mp_obj_is_callable(event_cb)) {
        mp_raise_TypeError(NULL);
    }

    bool result = false;
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    uint32_t count = lv_obj_get_event_count(obj);
    for (uint32_t i = 0; i < count; i++) {
        lv_event_dsc_t *dsc = lv_obj_get_event_dsc(obj, i);
        if (lv_event_dsc_get_cb(dsc) == lvgl_obj_event_cb) {
            gc_handle_t *user_data = lv_event_dsc_get_user_data(dsc);
            if (gc_handle_get(user_data) == MP_OBJ_TO_PTR(event_cb)) {
                result = lv_obj_remove_event(obj, i);
                gc_handle_free(user_data);
                break;
            }
        }
    }
    lvgl_unlock();
    return mp_obj_new_bool(result);
};
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lvgl_obj_remove_event_obj, lvgl_obj_remove_event);

STATIC mp_obj_t lvgl_obj_align_to(size_t n_args, const mp_obj_t *args) {
    lvgl_handle_t *handle = lvgl_obj_get(args[0], NULL);
    lvgl_handle_t *base_handle = lvgl_obj_get(args[1], NULL);
    lv_align_t align = mp_obj_get_int(args[2]);
    int32_t x_ofs = n_args > 3 ? mp_obj_get_int(args[3]) : 0;
    int32_t y_ofs = n_args > 4 ? mp_obj_get_int(args[4]) : 0;

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_obj_t *base_obj = lvgl_lock_obj(base_handle);
    lv_obj_align_to(obj, base_obj, align, x_ofs, y_ofs);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_obj_align_to_obj, 3, 5, lvgl_obj_align_to);

STATIC mp_obj_t lvgl_obj_align_as(size_t n_args, const mp_obj_t *args) {
    lvgl_handle_t *handle = lvgl_obj_get(args[0], NULL);
    lv_align_t align = mp_obj_get_int(args[1]);
    int32_t x_ofs = n_args > 2 ? mp_obj_get_int(args[2]) : 0;
    int32_t y_ofs = n_args > 3 ? mp_obj_get_int(args[3]) : 0;

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_obj_align(obj, align, x_ofs, y_ofs);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_obj_align_as_obj, 2, 4, lvgl_obj_align_as);

STATIC mp_obj_t lvgl_obj_add_style(size_t n_args, const mp_obj_t *args) {
    lv_style_selector_t selector;
    lvgl_handle_t *obj_handle = lvgl_obj_get(args[0], &selector);
    lvgl_style_handle_t *style_handle = lvgl_style_get(args[1]);
    if (n_args > 2) {
        selector = mp_obj_get_int(args[2]);
    }

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(obj_handle);
    lv_obj_add_style(obj, &style_handle->style, selector);
    lvgl_ptr_copy(&style_handle->base);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_obj_add_style_obj, 2, 3, lvgl_obj_add_style);

STATIC mp_obj_t lvgl_obj_replace_style(size_t n_args, const mp_obj_t *args) {
    lv_style_selector_t selector;
    lvgl_handle_t *obj_handle = lvgl_obj_get(args[0], &selector);
    lvgl_style_handle_t *old_handle = lvgl_style_get(args[1]);
    lvgl_style_handle_t *new_handle = lvgl_style_get(args[2]);
    if (n_args > 3) {
        selector = mp_obj_get_int(args[3]);
    }

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(obj_handle);
    int ref_count = lvgl_obj_preremove_style(obj, &old_handle->style, selector);
    lv_obj_replace_style(obj, &old_handle->style, &new_handle->style, selector);
    while (ref_count--) {
        lvgl_ptr_copy(&new_handle->base);
    }
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_obj_replace_style_obj, 3, 4, lvgl_obj_replace_style);

STATIC mp_obj_t lvgl_obj_remove_style(size_t n_args, const mp_obj_t *args) {
    lv_style_selector_t selector;
    lvgl_handle_t *obj_handle = lvgl_obj_get(args[0], &selector);
    lvgl_style_handle_t *style_handle = lvgl_style_get(args[1]);
    if (n_args > 2) {
        selector = mp_obj_get_int(args[2]);
    }

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(obj_handle);
    lvgl_obj_preremove_style(obj, &style_handle->style, selector);
    lv_obj_remove_style(obj, &style_handle->style, selector);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_obj_remove_style_obj, 2, 3, lvgl_obj_remove_style);

// STATIC mp_obj_t lvgl_obj_remove_style_all(mp_obj_t self_in) {
//     lvgl_obj_t *self = lvgl_obj_get(self_in, NULL);

//     lvgl_lock();
//     lv_obj_t *obj = lvgl_lock_obj(self);
//     lvgl_obj_preremove_style(obj, NULL, LV_PART_ANY | LV_STATE_ANY);
//     lv_obj_remove_style_all(obj);
//     lvgl_unlock();
//     return mp_const_none;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_obj_remove_style_all_obj, lvgl_obj_remove_style_all);

STATIC const mp_rom_map_elem_t lvgl_obj_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_ptr_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&lvgl_obj_delete_obj) },
    { MP_ROM_QSTR(MP_QSTR_update),          MP_ROM_PTR(&lvgl_obj_update_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_flag),     MP_ROM_PTR(&lvgl_obj_update_flag_obj) },
    { MP_ROM_QSTR(MP_QSTR_update_state),    MP_ROM_PTR(&lvgl_obj_update_state_obj) },
    { MP_ROM_QSTR(MP_QSTR_add_event),       MP_ROM_PTR(&lvgl_obj_add_event_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove_event),    MP_ROM_PTR(&lvgl_obj_remove_event_obj) },
    { MP_ROM_QSTR(MP_QSTR_align_to),        MP_ROM_PTR(&lvgl_obj_align_to_obj) },
    { MP_ROM_QSTR(MP_QSTR_align_as),        MP_ROM_PTR(&lvgl_obj_align_as_obj) },
    { MP_ROM_QSTR(MP_QSTR_add_style),       MP_ROM_PTR(&lvgl_obj_add_style_obj) },
    { MP_ROM_QSTR(MP_QSTR_replace_style),   MP_ROM_PTR(&lvgl_obj_replace_style_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove_style),    MP_ROM_PTR(&lvgl_obj_remove_style_obj) },
    // { MP_ROM_QSTR(MP_QSTR_remove_style_all),MP_ROM_PTR(&lvgl_obj_remove_style_all_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_obj_locals_dict, lvgl_obj_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_obj,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    // unary_op, lvgl_obj_unary_op,
    attr, lvgl_obj_attr,
    subscr, lvgl_obj_subscr,
    locals_dict, &lvgl_obj_locals_dict,
    protocol, &lv_obj_class
    );
MP_REGISTER_OBJECT(lvgl_type_obj);

const lvgl_ptr_type_t lvgl_obj_type = {
    &lvgl_type_obj,
    lvgl_obj_new,
    NULL,
    lvgl_obj_get_handle0,
    NULL,
};

void lvgl_obj_attr_int(lvgl_handle_t *handle, qstr attr, int32_t (*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, int32_t value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest) {
    lvgl_super_attr_check(attr, getter, setter, deleter, dest);
    int32_t value = 0;
    if (dest[1] != MP_OBJ_NULL) {
        value = mp_obj_get_int(dest[1]);
    }
    
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    if (dest[0] != MP_OBJ_SENTINEL) {
        value = getter(obj);
        lvgl_unlock();
        dest[0] = mp_obj_new_int(value);
        return;
    }
    else if (dest[1] != MP_OBJ_NULL) {
        setter(obj, value);
        dest[0] = MP_OBJ_NULL;
    }
    else {
        deleter(obj);
        dest[0] = MP_OBJ_NULL;
    }
    lvgl_unlock();
}

void lvgl_obj_attr_str(lvgl_handle_t *handle, qstr attr, char *(*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, const char *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest) {
    lvgl_super_attr_check(attr, getter, setter, deleter, dest);
    const char *value = NULL;
    if (dest[1] != MP_OBJ_NULL) {
        value = mp_obj_str_get_str(dest[1]);
    }
    
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    if (dest[0] != MP_OBJ_SENTINEL) {
        value = getter(obj);
        lvgl_unlock();
        dest[0] = value ? mp_obj_new_str_copy(&mp_type_str, (const unsigned char*)value, strlen(value)) : mp_const_none;
        return;
    }
    else if (dest[1] != MP_OBJ_NULL) {
        setter(obj, value);
        dest[0] = MP_OBJ_NULL;
    }
    else {
        deleter(obj);
        dest[0] = MP_OBJ_NULL;
    }
    lvgl_unlock();
}

void lvgl_obj_attr_obj(lvgl_handle_t *handle, qstr attr, lv_obj_t *(*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, lv_obj_t *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest) {
    lvgl_super_attr_check(attr, getter, setter, deleter, dest);
    lvgl_handle_t *value_handle = NULL;
    if (dest[1] != MP_OBJ_NULL) {
        value_handle = lvgl_obj_get_checked(dest[1]);
    }
    
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    if (dest[0] != MP_OBJ_SENTINEL) {
        lv_obj_t *value_obj = getter(obj);
        value_handle = lvgl_obj_get_handle(value_obj);
        dest[0] = lvgl_unlock_ptr(&value_handle->base);
        return;
    }
    else if (dest[1] != MP_OBJ_NULL) {
        lv_obj_t *value_obj = lvgl_lock_obj(value_handle);
        setter(obj, value_obj);
        dest[0] = MP_OBJ_NULL;
    }
    else {
        deleter(obj);
        dest[0] = MP_OBJ_NULL;
    }
    lvgl_unlock();
}

static void lvgl_obj_attr_style_prop(lvgl_handle_t *handle, lv_style_prop_t prop, mp_obj_t *dest, lv_style_selector_t selector, lv_type_code_t type) {
    lv_style_value_t new_value;
    if (dest[1] != MP_OBJ_NULL) {
        new_value = lvgl_style_value_from_mp(type, dest[1]);
    }
    
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_style_value_t old_value;
    bool has_old_value = lv_obj_get_local_style_prop(obj, prop, &old_value, selector) == LV_RESULT_OK;
    if (dest[0] != MP_OBJ_SENTINEL) {
        lv_style_value_t value = lv_obj_get_style_prop(obj, selector, prop);
        lvgl_unlock();
        dest[0] = lvgl_style_value_to_mp(type, value);
    }
    else if (dest[1] != MP_OBJ_NULL) {
        lv_obj_set_local_style_prop(obj, prop, new_value, selector);
        lvgl_unlock();
        if (has_old_value) {
            lvgl_style_value_free(type, old_value);
        }
        dest[0] = MP_OBJ_NULL;
    }
    else {
        bool removed = lv_obj_remove_local_style_prop(obj, prop, selector);
        lvgl_unlock();
        if (removed) {
            if (has_old_value) {
                lvgl_style_value_free(type, old_value);
            }            
            dest[0] = MP_OBJ_NULL;
        }
    }
}

static void lvgl_obj_del_event(void *arg) {
    lvgl_obj_event_t *event = arg;
    gc_handle_free(event->func);
    lvgl_ptr_delete(&event->current_target->base);
    lvgl_ptr_delete(&event->target->base);
    free(event);
}

// static const qstr lvgl_event_attrs[] = { MP_QSTR_target, MP_QSTR_code };

static void lvgl_obj_run_event(void *arg) {
    const qstr lvgl_event_attrs[] = { MP_QSTR_current_target, MP_QSTR_target, MP_QSTR_code };

    lvgl_obj_event_t *event = arg;
    mp_obj_t func = gc_handle_get(event->func);
    if (func == MP_OBJ_NULL) {
        return;
    }
    // lvgl_lock();
    // lv_obj_t *lv_obj = lvgl_obj_to_lv(event->target->lv_obj);
    // lvgl_unlock();
    // if (!lv_obj) {
    //     return;
    // }

    mp_obj_t items[] = { 
        lvgl_obj_to_mp(event->current_target), 
        lvgl_obj_to_mp(event->target), 
        MP_OBJ_NEW_SMALL_INT(event->code),
    };
    mp_obj_t e = mp_obj_new_attrtuple(lvgl_event_attrs, 3, items);
    mp_call_function_1(func, e);
}

static void lvgl_obj_event_cb(lv_event_t *e) {
    assert(lvgl_is_locked());

    lvgl_queue_t *queue = lvgl_queue_default;

    lvgl_obj_event_t *event = malloc(sizeof(lvgl_obj_event_t));
    event->elem.run = lvgl_obj_run_event;
    event->elem.del = lvgl_obj_del_event;
    
    gc_handle_t *func = lv_event_get_user_data(e);
    event->func = gc_handle_copy(func);

    lv_obj_t *current_target_obj = lv_event_get_current_target(e);
    lvgl_handle_t *current_target = lvgl_obj_get_handle(current_target_obj);
    event->current_target = lvgl_obj_copy(current_target);

    lv_obj_t *target_obj = lv_event_get_target(e);
    lvgl_handle_t *target = lvgl_obj_get_handle(target_obj);
    event->target = lvgl_obj_copy(target);
    
    event->code = lv_event_get_code(e);
    
    lvgl_queue_send(queue, &event->elem);
}


STATIC lvgl_handle_t *lvgl_obj_list_get(mp_obj_t self_in) {
    lvgl_obj_t *self = MP_OBJ_TO_PTR(self_in) - offsetof(lvgl_obj_t, children);
    return lvgl_ptr_from_mp(NULL, MP_OBJ_FROM_PTR(self));
}

STATIC mp_obj_t lvgl_obj_list_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    lvgl_handle_t *handle = lvgl_obj_list_get(self_in);
    if (op == MP_UNARY_OP_LEN) {
        lvgl_lock();
        lv_obj_t *obj = lvgl_lock_obj(handle);
        uint32_t count = lv_obj_get_child_count(obj);
        lvgl_unlock();
        return MP_OBJ_NEW_SMALL_INT(count);
    }

    return MP_OBJ_NULL;
}

STATIC mp_obj_t lvgl_obj_list_subscr(mp_obj_t self_in, mp_obj_t index_in, mp_obj_t value) {
    lvgl_handle_t *handle = lvgl_obj_list_get(self_in);
    lvgl_super_subscr_check(mp_obj_get_type(self_in), true, false, false, value);
    mp_int_t index = mp_obj_get_int(index_in);
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_obj_t *child_obj = lv_obj_get_child(obj, index);
    if (!child_obj) {
        lvgl_unlock();
        mp_raise_type(&mp_type_IndexError);
    }
    lvgl_handle_t *child_handle = lvgl_obj_get_handle(child_obj);
    return lvgl_unlock_ptr(&child_handle->base);
}

STATIC mp_obj_t lvgl_obj_list_tuple(mp_obj_t self_in) {
    size_t len = MP_OBJ_SMALL_INT_VALUE(lvgl_obj_list_unary_op(MP_UNARY_OP_LEN, self_in));
    mp_obj_t tuple_in = mp_obj_new_tuple(len, NULL);
    mp_obj_tuple_t *tuple = MP_OBJ_TO_PTR(tuple_in);
    for (size_t idx = 0; idx < len; idx++) {
        tuple->items[idx] = lvgl_obj_list_subscr(self_in, MP_OBJ_NEW_SMALL_INT(idx), MP_OBJ_SENTINEL);
    }
    return tuple_in;
}

STATIC mp_obj_t lvgl_obj_list_getiter(mp_obj_t self_in,  mp_obj_iter_buf_t *iter_buf) {
    mp_obj_t tuple = lvgl_obj_list_tuple(self_in);
    return mp_obj_tuple_getiter(tuple, iter_buf);
}

STATIC mp_obj_t lvgl_obj_list_clear(mp_obj_t self_in) {
    lvgl_handle_t *handle = lvgl_obj_list_get(self_in);
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_obj_clean(obj);
    lvgl_unlock();
    return mp_const_none;
};
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_obj_list_clear_obj, lvgl_obj_list_clear);

STATIC const mp_rom_map_elem_t lvgl_obj_list_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_clear),          MP_ROM_PTR(&lvgl_obj_list_clear_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_obj_list_locals_dict, lvgl_obj_list_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_obj_list,
    MP_ROM_QSTR_CONST(MP_QSTR_ObjectCollection),
    MP_TYPE_FLAG_ITER_IS_GETITER,
    // attr, lvgl_obj_list_attr,
    unary_op, lvgl_obj_list_unary_op,
    subscr, lvgl_obj_list_subscr,
    iter, lvgl_obj_list_getiter,
    locals_dict, &lvgl_obj_list_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_obj_list);
