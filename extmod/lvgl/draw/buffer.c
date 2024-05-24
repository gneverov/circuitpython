// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./buffer.h"
#include "../modlvgl.h"
#include "../types.h"

#include "py/binary.h"
#include "py/runtime.h"


STATIC mp_obj_t lvgl_draw_buf_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 3, 4, false);
    uint32_t w = mp_obj_get_int(args[0]);
    uint32_t h = mp_obj_get_int(args[1]);
    lv_color_format_t cf = mp_obj_get_int(args[2]);
    uint32_t stride = 0;
    if (n_args > 3) {
        stride = mp_obj_get_int(args[3]);
    }

    lvgl_lock();
    lv_draw_buf_t *draw_buf = lv_draw_buf_create(w, h, cf, stride);
    if (!draw_buf) {
        lvgl_unlock();
        mp_raise_type(&mp_type_MemoryError);
    }
    lv_draw_buf_clear(draw_buf, NULL);
    assert(lv_draw_buf_has_flag(draw_buf, LV_IMAGE_FLAGS_ALLOCATED));
    lvgl_unlock();

    lvgl_draw_buf_handle_t *handle = malloc(sizeof(lvgl_draw_buf_handle_t));
    lvgl_ptr_init_handle(&handle->base, &lvgl_draw_buf_type, draw_buf);
    draw_buf->user_data = handle;
    return lvgl_ptr_to_mp(&handle->base);
}

STATIC mp_int_t lvgl_draw_buf_get_buffer(mp_obj_t self_in, mp_buffer_info_t *bufinfo, mp_uint_t flags) {
    lvgl_draw_buf_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    lv_draw_buf_t *draw_buf = lvgl_draw_buf_to_lv(handle);
    if (!draw_buf) {
        return -1;
    }
    lvgl_lock();
    bufinfo->typecode = BYTEARRAY_TYPECODE;
    bufinfo->buf = draw_buf->data;
    bufinfo->len = draw_buf->data_size;
    lvgl_unlock();
    return 0;
}

STATIC const mp_rom_map_elem_t lvgl_draw_buf_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),      MP_ROM_PTR(&lvgl_ptr_del_obj) },
};
STATIC MP_DEFINE_CONST_DICT(lvgl_draw_buf_locals_dict_table_obj, lvgl_draw_buf_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_draw_buf,
    MP_ROM_QSTR_CONST(MP_QSTR_Buffer),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_draw_buf_make_new,
    buffer, lvgl_draw_buf_get_buffer,
    locals_dict, &lvgl_draw_buf_locals_dict_table_obj
    );
MP_REGISTER_OBJECT(lvgl_type_draw_buf);

static lvgl_ptr_t lvgl_draw_buf_get_handle0(const void *lv_ptr) {
    const lv_draw_buf_t *draw_buf = lv_ptr;
    return draw_buf->user_data;
}

static void lvgl_draw_buf_deinit(lvgl_ptr_t ptr) {
    lvgl_draw_buf_handle_t *handle = ptr;
    lv_draw_buf_t *draw_buf = lvgl_draw_buf_to_lv(handle);
    lv_draw_buf_destroy(draw_buf);
}

const lvgl_ptr_type_t lvgl_draw_buf_type = {
    &lvgl_type_draw_buf,
    NULL,
    lvgl_draw_buf_deinit,
    lvgl_draw_buf_get_handle0,
    NULL,
};
