// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./canvas.h"
#include "../draw/buffer.h"
#include "../modlvgl.h"
#include "../obj.h"
#include "../super.h"

#include "py/runtime.h"


extern const mp_obj_type_t lvgl_type_canvas_layer;

static mp_obj_t lvgl_canvas_layer_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args);

__attribute__ ((visibility("hidden")))
void lvgl_canvas_event_delete(lv_obj_t *obj) {
    assert(lvgl_is_locked());
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(obj);
    lvgl_draw_buf_handle_t *handle = lvgl_draw_buf_from_lv(draw_buf);
    lvgl_ptr_delete(&handle->base);
}

static void lvgl_canvas_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_super_attr(self_in, &lvgl_type_canvas, attr, dest);
}

static mp_obj_t lvgl_canvas_set_buffer(mp_obj_t self_in, mp_obj_t buffer_in) {
    lvgl_obj_handle_t *obj_handle = lvgl_obj_from_mp(self_in, NULL);
    lvgl_draw_buf_handle_t *new_handle = lvgl_ptr_from_mp(&lvgl_draw_buf_type, buffer_in);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(obj_handle);
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(obj);
    lvgl_draw_buf_handle_t *old_handle = lvgl_draw_buf_from_lv(draw_buf);

    draw_buf = lvgl_draw_buf_to_lv(new_handle);
    lv_canvas_set_draw_buf(obj, draw_buf);
    lvgl_ptr_copy(&new_handle->base);
    lvgl_unlock();

    lvgl_ptr_delete(&old_handle->base);

    return mp_const_none;    
}
static MP_DEFINE_CONST_FUN_OBJ_2(lvgl_canvas_set_buffer_obj, lvgl_canvas_set_buffer);

static mp_obj_t lvgl_canvas_get_buffer(mp_obj_t self_in) {
    lvgl_obj_handle_t *obj_handle = lvgl_obj_from_mp(self_in, NULL);

    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(obj_handle);
    lv_draw_buf_t *draw_buf = lv_canvas_get_draw_buf(obj);
    lvgl_draw_buf_handle_t *buf_handle = lvgl_draw_buf_from_lv(draw_buf);
    return lvgl_unlock_ptr(&buf_handle->base);
}
static MP_DEFINE_CONST_FUN_OBJ_1(lvgl_canvas_get_buffer_obj, lvgl_canvas_get_buffer);

static mp_obj_t lvgl_canvas_layer(mp_obj_t self_in) {
    return lvgl_canvas_layer_make_new(&lvgl_type_canvas_layer, 1, 0, &self_in);
}
static MP_DEFINE_CONST_FUN_OBJ_1(lvgl_canvas_layer_obj, lvgl_canvas_layer);

static mp_obj_t lvgl_canvas_set_px(size_t n_args, const mp_obj_t *args) {
    mp_obj_t self_in = args[0];
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);
    int32_t x = mp_obj_get_int(args[1]);
    int32_t y = mp_obj_get_int(args[2]);
    lv_color_t color = lv_color_hex(mp_obj_get_int(args[3]));
    lv_opa_t opa = LV_OPA_COVER;
    if (n_args > 4) {
        opa = mp_obj_get_int(args[4]);
    }
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_canvas_set_px(obj, x, y, color, opa);
    lvgl_unlock();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_canvas_set_px_obj, 4, 5, lvgl_canvas_set_px);

// void lv_canvas_set_palette(lv_obj_t * obj, uint8_t id, lv_color32_t c)

static mp_obj_t lvgl_canvas_fill_bg(size_t n_args, const mp_obj_t *args) {
    mp_obj_t self_in = args[0];
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);
    lv_color_t color = lv_color_hex(mp_obj_get_int(args[1]));
    lv_opa_t opa = LV_OPA_COVER;
    if (n_args > 2) {
        opa = mp_obj_get_int(args[2]);
    }
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_canvas_fill_bg(obj, color, opa);
    lvgl_unlock();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_canvas_fill_bg_obj, 2, 3, lvgl_canvas_fill_bg);

static mp_obj_t lvgl_canvas_get_px(mp_obj_t self_in, mp_obj_t x_in, mp_obj_t y_in) {
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self_in, NULL);
    int32_t x = mp_obj_get_int(x_in);
    int32_t y = mp_obj_get_int(y_in);
    lvgl_lock();
    lv_obj_t *obj = lvgl_lock_obj(handle);
    lv_color32_t color = lv_canvas_get_px(obj, x, y);
    lvgl_unlock();
    return mp_obj_new_int(*(mp_int_t *)&color);
}
static MP_DEFINE_CONST_FUN_OBJ_3(lvgl_canvas_get_px_obj, lvgl_canvas_get_px);

static const mp_rom_map_elem_t lvgl_canvas_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_set_buffer),      MP_ROM_PTR(&lvgl_canvas_set_buffer_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_buffer),      MP_ROM_PTR(&lvgl_canvas_get_buffer_obj) },
    { MP_ROM_QSTR(MP_QSTR_layer),           MP_ROM_PTR(&lvgl_canvas_layer_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_px),          MP_ROM_PTR(&lvgl_canvas_set_px_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill_bg),         MP_ROM_PTR(&lvgl_canvas_fill_bg_obj) },
    { MP_ROM_QSTR(MP_QSTR_get_px),          MP_ROM_PTR(&lvgl_canvas_get_px_obj) },
};
static MP_DEFINE_CONST_DICT(lvgl_canvas_locals_dict, lvgl_canvas_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_canvas,
    MP_ROM_QSTR_CONST(MP_QSTR_Canvas),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_obj_make_new,
    attr, lvgl_canvas_attr,
    subscr, lvgl_obj_subscr,
    parent, &lvgl_type_obj,
    protocol, &lv_canvas_class,
    locals_dict, &lvgl_canvas_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_canvas);


static mp_obj_t lvgl_canvas_layer_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    lvgl_obj_canvas_layer_t *self = mp_obj_malloc_with_finaliser(lvgl_obj_canvas_layer_t, type);
    lvgl_layer_init(&self->base);
    self->canvas = args[0];
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t lvgl_canvas_layer_enter(mp_obj_t self_in) {
    lvgl_obj_canvas_layer_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->base.layer) {
        mp_raise_ValueError(NULL);
    }
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self->canvas, NULL);
    lvgl_lock();    
    lv_obj_t *obj = lvgl_lock_obj(handle);    
    lv_canvas_init_layer(obj, &self->layer);
    self->base.layer = &self->layer;
    lvgl_unlock();
    return self_in;
}
static MP_DEFINE_CONST_FUN_OBJ_1(lvgl_canvas_layer_enter_obj, lvgl_canvas_layer_enter);

static mp_obj_t lvgl_canvas_layer_exit(size_t n_args, const mp_obj_t *args) {
    lvgl_obj_canvas_layer_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->base.layer) {
        mp_raise_ValueError(NULL);
    }
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp(self->canvas, NULL);
    lvgl_lock();    
    lv_obj_t *obj = lvgl_lock_obj(handle); 
    lv_canvas_finish_layer(obj, &self->layer);
    lvgl_unlock();
    self->base.layer = NULL;
    return mp_const_none;    
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_canvas_layer_exit_obj, 1, 4, lvgl_canvas_layer_exit);

static const mp_rom_map_elem_t lvgl_canvas_layer_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___enter__),       MP_ROM_PTR(&lvgl_canvas_layer_enter_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__),        MP_ROM_PTR(&lvgl_canvas_layer_exit_obj) },
};
static MP_DEFINE_CONST_DICT(lvgl_canvas_layer_locals_dict, lvgl_canvas_layer_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_canvas_layer,
    MP_ROM_QSTR_CONST(MP_QSTR_CanvasLayer),
    MP_TYPE_FLAG_NONE,
    locals_dict, &lvgl_canvas_layer_locals_dict,
    parent, &lvgl_type_layer
    );
MP_REGISTER_OBJECT(lvgl_type_canvas_layer);
