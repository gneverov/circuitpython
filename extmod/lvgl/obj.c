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


static void lvgl_obj_attr_style_prop(lvgl_obj_t *self, lv_style_prop_t prop, mp_obj_t *dest);

static void lvgl_obj_event_delete(lv_event_t *e);

static void lvgl_obj_event_cb(lv_event_t *e);

lvgl_handle_t *lvgl_handle_alloc(lv_obj_t *obj) {
    assert(lvgl_is_locked());
    assert(!lv_obj_get_user_data(obj));

    lvgl_handle_t *handle = malloc(sizeof(lvgl_handle_t));
    handle->ref_count = 1;
    handle->lv_obj = obj;
    handle->mp_obj = NULL;

    const lv_obj_class_t *obj_type = lv_obj_get_class(obj);
    handle->type = lvgl_obj_class_from(obj_type);  

    lv_obj_set_user_data(obj, handle);
    lv_obj_add_event_cb(obj, lvgl_obj_event_delete, LV_EVENT_DELETE, NULL);

    return handle;
}

lvgl_handle_t *lvgl_handle_copy(lvgl_handle_t *handle) {
    assert(lvgl_is_locked());
    assert(handle->ref_count > 0);
    handle->ref_count++;
    return handle;
}

void lvgl_handle_free(lvgl_handle_t *handle) {
    assert(lvgl_is_locked());
    assert(handle->ref_count > 0);
    if (--handle->ref_count == 0) {
        free(handle);
    }
}

lvgl_handle_t *lvgl_obj_get_handle(lv_obj_t *obj) {
    assert(lvgl_is_locked());
    lvgl_handle_t *handle = lv_obj_get_user_data(obj);
    if (!handle) {
        handle = lvgl_handle_alloc(obj);
        lv_obj_set_user_data(obj, handle);
    }
    assert(handle->lv_obj == obj);
    return handle;
}

lvgl_obj_t *lvgl_handle_get_obj(lvgl_handle_t *handle) {
    lvgl_obj_t *self = handle->mp_obj;
    if (!self) {
        self = m_new_obj_with_finaliser(lvgl_obj_t);
        lvgl_lock();
        self->base.type = handle->type;
        self->handle = lvgl_handle_copy(handle);
        mp_map_init(&self->members, 0);
        self->children.type = &lvgl_type_obj_list;
        handle->mp_obj = self;
        lvgl_unlock();
    }
    return self;
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

    lvgl_handle_t *handle = lv_obj_get_user_data(obj);
    if (handle) {
        assert(obj == handle->lv_obj);

        handle->lv_obj = NULL;
        lvgl_handle_free(handle);

        lv_obj_set_user_data(obj, NULL);
    }
}

lvgl_obj_t *lvgl_obj_get(mp_obj_t self_in) {
    return MP_OBJ_TO_PTR(self_in);
}

lvgl_obj_t *lvgl_obj_get_checked(mp_obj_t self_in) {
    const mp_obj_type_t *type = mp_obj_get_type(self_in);
    if (!mp_obj_is_subclass_fast(MP_OBJ_FROM_PTR(type), MP_OBJ_FROM_PTR(&lvgl_type_obj))) {
        mp_raise_msg_varg(&mp_type_TypeError, MP_ERROR_TEXT("'%q' object isn't an lvgl object"), (qstr)type->name);
    }    
    return MP_OBJ_TO_PTR(self_in);
}

STATIC lv_obj_t *lvgl_lock_obj(lvgl_obj_t *self) {
    assert(lvgl_is_locked());
    assert(self->handle->mp_obj == self);
    lv_obj_t *obj = self->handle->lv_obj;
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
    const lv_obj_class_t *type_obj = MP_OBJ_TYPE_GET_SLOT(type, protocol);

    mp_obj_t parent_in = n_args > 0 ? args[0] : MP_OBJ_NULL;
    lvgl_obj_t *parent = NULL;
    if ((parent_in != MP_OBJ_NULL) && (parent_in != mp_const_none)) {
        parent = lvgl_obj_get_checked(parent_in);
    }

    lvgl_lock_init();
    lv_obj_t *parent_obj = NULL;
    if (parent) {
        parent_obj = lvgl_lock_obj(parent);
    }
    else if (!parent_in) {
        parent_obj = lv_screen_active();
    }
    lv_obj_t *obj = lv_obj_class_create_obj(type_obj, parent_obj);
    lv_obj_class_init_obj(obj);
    lvgl_handle_t *handle = lvgl_obj_get_handle(obj);
    lvgl_unlock();
    lvgl_obj_t *self = lvgl_handle_get_obj(handle);

    self->members.is_fixed = 1;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        const mp_map_elem_t *kw = (mp_map_elem_t *)(args + n_args);
        const mp_map_elem_t *const kw_end = kw + n_kw;
        for (; kw < kw_end; kw++) {
            mp_store_attr(MP_OBJ_FROM_PTR(self), mp_obj_str_get_qstr(kw->key), kw->value);
        }
        nlr_pop();
    }
    else {
        lvgl_lock();
        if (handle->lv_obj) {
            lv_obj_delete(handle->lv_obj);
        }
        lvgl_unlock();
        nlr_raise(nlr.ret_val);
    }

    self->members.is_fixed = 0;
    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t lvgl_obj_del(mp_obj_t self_in) {
    lvgl_obj_t *self = lvgl_obj_get(self_in);

    lvgl_handle_t *handle = self->handle;
    if (handle) {
        lvgl_lock();
        assert(handle->mp_obj = self);
        handle->mp_obj = NULL;
        lvgl_handle_free(handle);
        lvgl_unlock();
        self->handle = NULL;
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_obj_del_obj, lvgl_obj_del);

void lvgl_obj_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_obj_t *self = lvgl_obj_get(self_in);
    if (attr == MP_QSTR_children) {
        lvgl_super_attr_check(attr, true, false, false, dest);
        dest[0] = MP_OBJ_FROM_PTR(&self->children);
        return;
    }
    else if (attr == MP_QSTR_index) {
        lvgl_obj_attr_int(self, attr, lv_obj_get_index, NULL, NULL, dest);
        return;
    }
    else if (attr == MP_QSTR_parent) {
        lvgl_obj_attr_obj(self, attr, lv_obj_get_parent, lv_obj_set_parent, NULL, dest);
        return;
    }

    lv_style_prop_t prop = lvgl_style_lookup(attr);
    if (prop) {
        lvgl_obj_attr_style_prop(self, prop, dest);    
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

STATIC mp_obj_t lvgl_obj_delete(mp_obj_t self_in) {
    lvgl_obj_t *self = lvgl_obj_get(self_in);
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    lv_obj_delete(obj);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_obj_delete_obj, lvgl_obj_delete);

STATIC mp_obj_t lvgl_obj_add_event(mp_obj_t self_in, mp_obj_t event_cb, mp_obj_t filter_in) {
    lvgl_obj_t *self = lvgl_obj_get(self_in);
    if (!mp_obj_is_callable(event_cb)) {
        mp_raise_TypeError(NULL);
    }
    mp_int_t filter = mp_obj_get_int(filter_in);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    gc_handle_t *user_data = gc_handle_alloc(MP_OBJ_TO_PTR(event_cb));
    lv_obj_add_event_cb(obj, lvgl_obj_event_cb, filter, user_data);
    lvgl_unlock();
    return mp_const_none;
};
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lvgl_obj_add_event_obj, lvgl_obj_add_event);

STATIC mp_obj_t lvgl_obj_remove_event(mp_obj_t self_in, mp_obj_t event_cb) {
    lvgl_obj_t *self = lvgl_obj_get(self_in);
    if (!mp_obj_is_callable(event_cb)) {
        mp_raise_TypeError(NULL);
    }

    bool result = false;
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
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
    lvgl_obj_t *self = lvgl_obj_get(args[0]);
    lvgl_obj_t *base = lvgl_obj_get(args[1]);
    lv_align_t align = mp_obj_get_int(args[2]);
    int32_t x_ofs = mp_obj_get_int(args[3]);
    int32_t y_ofs = mp_obj_get_int(args[4]);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    lv_obj_t *base_obj = lvgl_lock_obj(base);
    lv_obj_align_to(obj, base_obj, align, x_ofs, y_ofs);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_obj_align_to_obj, 5, 5, lvgl_obj_align_to);

STATIC mp_obj_t lvgl_obj_remove_style(mp_obj_t self_in, mp_obj_t style_in) {
    lvgl_obj_t *self = lvgl_obj_get(self_in);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    lv_obj_remove_style_all(obj);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lvgl_obj_remove_style_obj, lvgl_obj_remove_style);

STATIC const mp_rom_map_elem_t lvgl_obj_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_obj_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&lvgl_obj_delete_obj) },
    { MP_ROM_QSTR(MP_QSTR_add_event),       MP_ROM_PTR(&lvgl_obj_add_event_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove_event),    MP_ROM_PTR(&lvgl_obj_remove_event_obj) },
    { MP_ROM_QSTR(MP_QSTR_align_to),        MP_ROM_PTR(&lvgl_obj_align_to_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove_style),    MP_ROM_PTR(&lvgl_obj_remove_style_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_obj_locals_dict, lvgl_obj_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_obj,
    MP_QSTR_object,
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    // unary_op, lvgl_obj_unary_op,
    attr, lvgl_obj_attr,
    // subscr, lvgl_obj_subscr,
    locals_dict, &lvgl_obj_locals_dict,
    protocol, &lv_obj_class
    );
MP_REGISTER_OBJECT(lvgl_type_obj);

void lvgl_obj_attr_int(lvgl_obj_t *self, qstr attr, int32_t (*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, int32_t value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest) {
    lvgl_super_attr_check(attr, getter, setter, deleter, dest);
    int32_t value = 0;
    if (dest[1] != MP_OBJ_NULL) {
        value = mp_obj_get_int(dest[1]);
    }
    
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    if (dest[0] != MP_OBJ_SENTINEL) {
        value = getter(obj);
        dest[0] = mp_obj_new_int(value);
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

void lvgl_obj_attr_str(lvgl_obj_t *self, qstr attr, char *(*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, const char *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest) {
    lvgl_super_attr_check(attr, getter, setter, deleter, dest);
    const char *value = NULL;
    if (dest[1] != MP_OBJ_NULL) {
        value = mp_obj_str_get_str(dest[1]);
    }
    
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    if (dest[0] != MP_OBJ_SENTINEL) {
        value = getter(obj);
        if (value) {
            lvgl_unlock();
            dest[0] = mp_obj_new_str_copy(&mp_type_str, (const unsigned char*)value, strlen(value));
            return;
        }
        dest[0] = mp_const_none;
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

void lvgl_obj_attr_obj(lvgl_obj_t *self, qstr attr, lv_obj_t *(*getter)(const lv_obj_t *obj), void (*setter)(lv_obj_t *obj, lv_obj_t *value), void (*deleter)(lv_obj_t *obj), mp_obj_t *dest) {
    lvgl_super_attr_check(attr, getter, setter, deleter, dest);
    lvgl_obj_t *value = NULL;
    if (dest[1] != MP_OBJ_NULL) {
        value = lvgl_obj_get_checked(dest[1]);
    }
    
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    if (dest[0] != MP_OBJ_SENTINEL) {
        lv_obj_t *value_obj = getter(obj);
        if (value_obj) {
            lvgl_handle_t *handle = lvgl_obj_get_handle(value_obj);
            lvgl_unlock();
            dest[0] = MP_OBJ_FROM_PTR(lvgl_handle_get_obj(handle));
            return;
        }
        dest[0] = mp_const_none;
    }
    else if (dest[1] != MP_OBJ_NULL) {
        lv_obj_t *value_obj = lvgl_lock_obj(value);
        setter(obj, value_obj);
        dest[0] = MP_OBJ_NULL;
    }
    else {
        deleter(obj);
        dest[0] = MP_OBJ_NULL;
    }
    lvgl_unlock();
}

void lvgl_obj_attr_style_prop(lvgl_obj_t *self, lv_style_prop_t prop, mp_obj_t *dest) {
    lv_style_value_t value = { .num = 0 };
    if (dest[1] != MP_OBJ_NULL) {
        value.num = mp_obj_get_int(dest[1]);
    }
    
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    if (dest[0] != MP_OBJ_SENTINEL) {
        value = lv_obj_get_style_prop(obj, LV_PART_MAIN, prop);
        dest[0] = mp_obj_new_int(value.num);
    }
    else if (dest[1] != MP_OBJ_NULL) {
        lv_obj_set_local_style_prop(obj, prop, value, LV_PART_MAIN);
        dest[0] = MP_OBJ_NULL;
    }
    else {
        lv_obj_remove_local_style_prop(obj, prop, LV_PART_MAIN);
        dest[0] = MP_OBJ_NULL;
    }
    lvgl_unlock();
}

static void lvgl_obj_del_event(void *arg) {
    lvgl_obj_event_t *event = arg;
    gc_handle_free(event->func);
    lvgl_handle_free(event->target);
    free(event);
}

// static const qstr lvgl_event_attrs[] = { MP_QSTR_target, MP_QSTR_code };

static void lvgl_obj_run_event(void *arg) {
    const qstr lvgl_event_attrs[] = { MP_QSTR_target, MP_QSTR_code };

    lvgl_obj_event_t *event = arg;
    mp_obj_t func = gc_handle_get(event->func);
    if (func == MP_OBJ_NULL) {
        return;
    }
    lvgl_lock();
    lv_obj_t *lv_obj = event->target->lv_obj;
    lvgl_unlock();
    if (!lv_obj) {
        return;
    }
    mp_obj_t mp_obj = MP_OBJ_FROM_PTR(lvgl_handle_get_obj(event->target));
    mp_obj_t code = MP_OBJ_NEW_SMALL_INT(event->code);

    mp_obj_t items[] = { mp_obj, code };
    mp_obj_t e = mp_obj_new_attrtuple(lvgl_event_attrs, 2, items);
    mp_call_function_1(func, e);
}

static void lvgl_obj_event_cb(lv_event_t *e) {
    assert(lvgl_is_locked());

    lvgl_queue_t *queue = lvgl_queue_default;
    if (!queue->obj) {
        return;
    }

    lvgl_obj_event_t *event = malloc(sizeof(lvgl_obj_event_t));
    event->elem.run = lvgl_obj_run_event;
    event->elem.del = lvgl_obj_del_event;
    
    gc_handle_t *func = lv_event_get_user_data(e);
    event->func = gc_handle_copy(func);

    lv_obj_t *target_obj = lv_event_get_target(e);
    lvgl_handle_t *target = lvgl_obj_get_handle(target_obj);
    event->target = lvgl_handle_copy(target);
    
    event->code = lv_event_get_code(e);
    
    lvgl_queue_send(queue, &event->elem);
}


STATIC lvgl_obj_t *lvgl_obj_list_get(mp_obj_t self_in) {
    lvgl_obj_t *self = MP_OBJ_TO_PTR(self_in) - offsetof(lvgl_obj_t, children);
    // assert(1);
    return self;
}

STATIC mp_obj_t lvgl_obj_list_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    lvgl_obj_t *self = lvgl_obj_list_get(self_in);
    if (op == MP_UNARY_OP_LEN) {
        lvgl_lock();
        lv_obj_t *obj = lvgl_lock_obj(self);
        uint32_t count = lv_obj_get_child_count(obj);
        lvgl_unlock();
        return MP_OBJ_NEW_SMALL_INT(count);
    }

    return MP_OBJ_NULL;
}

STATIC mp_obj_t lvgl_obj_list_subscr(mp_obj_t self_in, mp_obj_t index_in, mp_obj_t value) {
    lvgl_obj_t *self = lvgl_obj_list_get(self_in);
    lvgl_super_subscr_check(self->base.type, true, false, false, value);
    mp_int_t index = mp_obj_get_int(index_in);
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
    lv_obj_t *child_obj = lv_obj_get_child(obj, index);
    if (!child_obj) {
        lvgl_unlock();
        mp_raise_type(&mp_type_IndexError);
    }
    lvgl_handle_t *handle = lvgl_obj_get_handle(child_obj);
    lvgl_unlock();
    return MP_OBJ_FROM_PTR(lvgl_handle_get_obj(handle));
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
    lvgl_obj_t *self = lvgl_obj_list_get(self_in);
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(self);
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
