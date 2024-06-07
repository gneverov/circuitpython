// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./image_decoder.h"
#include "../modlvgl.h"
#include "../types.h"

#include "extmod/freeze/extmod.h"
#include "py/runtime.h"


static const qstr lvgl_image_header_attrs[] = { MP_ROM_QSTR_CONST(MP_QSTR_w), MP_ROM_QSTR_CONST(MP_QSTR_h) };
MP_REGISTER_STRUCT(lvgl_image_header_attrs, qstr);

static mp_obj_t lvgl_image_decoder_get_info(mp_obj_t src_in) {
    void *src = NULL;
    lvgl_type_from_mp(LV_TYPE_IMAGE_SRC, src_in, &src);

    lv_image_header_t header;
    lvgl_lock();
    lv_result_t res = lv_image_decoder_get_info(src, &header);
    lvgl_unlock();

    if (res != LV_RES_OK) {
        mp_raise_ValueError(NULL);
    }
    mp_obj_t items[] = {
        mp_obj_new_int(header.w),
        mp_obj_new_int(header.h),
    };
    return mp_obj_new_attrtuple(lvgl_image_header_attrs, 2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(lvgl_image_decoder_get_info_fun_obj, lvgl_image_decoder_get_info);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(lvgl_image_decoder_get_info_obj, MP_ROM_PTR(&lvgl_image_decoder_get_info_fun_obj));

// static void lvgl_image_decoder_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
// }

static const mp_rom_map_elem_t lvgl_image_decoder_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_get_info),        MP_ROM_PTR(&lvgl_image_decoder_get_info_obj) },
};
static MP_DEFINE_CONST_DICT(lvgl_image_decoder_locals_dict, lvgl_image_decoder_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_image_decoder,
    MP_ROM_QSTR_CONST(MP_QSTR_ImageDecoder),
    MP_TYPE_FLAG_NONE,
    // attr, lvgl_image_decoder_attr,
    locals_dict, &lvgl_image_decoder_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_image_decoder);
