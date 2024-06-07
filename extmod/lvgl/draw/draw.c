// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./buffer.h"
#include "./draw.h"
#include "./layer.h"
#include "../font.h"
#include "../misc.h"
#include "../modlvgl.h"
#include "../super.h"
#include "../types.h"

#include "extmod/freeze/extmod.h"


typedef struct lvgl_dsc_handle {
    lvgl_ptr_handle_t base;
    lv_draw_dsc_base_t dsc[];
} lvgl_dsc_handle_t;

static mp_obj_t lvgl_dsc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, true);
    assert(MP_OBJ_TYPE_HAS_SLOT(type, protocol));
    const lvgl_dsc_type_t *dsc_type = MP_OBJ_TYPE_GET_SLOT(type, protocol);
    lvgl_dsc_handle_t *handle = malloc(offsetof(lvgl_dsc_handle_t, dsc) + dsc_type->size);
    lvgl_ptr_init_handle(&handle->base, &dsc_type->base, handle->dsc);
    dsc_type->init(handle->dsc);
    handle->dsc->user_data = handle;
    mp_obj_t self_out = lvgl_ptr_to_mp(&handle->base);

    lvgl_super_update(self_out, n_kw, (mp_map_elem_t *)(args + n_args));
    return self_out;    
}

static mp_obj_t lvgl_dsc_reset(mp_obj_t self_in) {
    lvgl_dsc_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    const lvgl_dsc_type_t *dsc_type = (void *)handle->base.type;
    lvgl_attrs_free(dsc_type->base.attrs, handle->dsc);
    dsc_type->init(handle->dsc);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(lvgl_dsc_reset_obj, lvgl_dsc_reset);

static mp_obj_t lvgl_dsc_draw(size_t n_args, const mp_obj_t *args) {
    lvgl_dsc_handle_t *handle = lvgl_ptr_from_mp(NULL, args[0]);
    const lvgl_dsc_type_t *dsc_type = (void *)handle->base.type;

    lv_layer_t *layer = lvgl_layer_get(args[1]);

    if (n_args > 2) {
        if (!dsc_type->draw_coords) {
            mp_raise_TypeError(NULL);
        }
        lv_area_t coords;
        lvgl_area_from_mp(args[2], &coords);
        lvgl_lock();
        dsc_type->draw_coords(layer, handle->dsc, &coords);
        lvgl_unlock();        
    }
    else {
        if (!dsc_type->draw) {
            mp_raise_TypeError(NULL);
        }
        lvgl_lock();
        dsc_type->draw(layer, handle->dsc);
        lvgl_unlock();        
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(lvgl_dsc_draw_obj, 2, 3, lvgl_dsc_draw);

static const mp_rom_map_elem_t lvgl_dsc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_ptr_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset),           MP_ROM_PTR(&lvgl_dsc_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw),            MP_ROM_PTR(&lvgl_dsc_draw_obj) },
};
static MP_DEFINE_CONST_DICT(lvgl_dsc_locals_dict, lvgl_dsc_locals_dict_table);

static void lvgl_dsc_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_ptr_attr(self_in, attr, dest);
    if (dest[1] == MP_OBJ_SENTINEL) {
        lvgl_super_attr(self_in, &lvgl_type_dsc, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_dsc,
    MP_ROM_QSTR_CONST(MP_QSTR_Dsc),
    MP_TYPE_FLAG_NONE,
    attr, lvgl_dsc_attr,
    locals_dict, &lvgl_dsc_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_dsc);

static lvgl_ptr_t lvgl_dsc_get_handle(const void *value) {
    const lv_draw_dsc_base_t *dsc = value;
    return dsc->user_data;
}


static const lvgl_type_attr_t lvgl_arc_dsc_attrs[] = {
    { MP_ROM_QSTR_CONST(MP_QSTR_color), offsetof(lv_draw_arc_dsc_t, color), LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_width), offsetof(lv_draw_arc_dsc_t, width), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_start_angle), offsetof(lv_draw_arc_dsc_t, start_angle), LV_TYPE_FLOAT },
    { MP_ROM_QSTR_CONST(MP_QSTR_end_angle), offsetof(lv_draw_arc_dsc_t, end_angle), LV_TYPE_FLOAT },
    { MP_ROM_QSTR_CONST(MP_QSTR_center_x), offsetof(lv_draw_arc_dsc_t, center) + offsetof(lv_point_t, x), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_center_y), offsetof(lv_draw_arc_dsc_t, center) + offsetof(lv_point_t, y), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_radius), offsetof(lv_draw_arc_dsc_t, radius), LV_TYPE_INT16 },
    { MP_ROM_QSTR_CONST(MP_QSTR_opa), offsetof(lv_draw_arc_dsc_t, opa), LV_TYPE_INT8 },
    { 0 },
 };
MP_REGISTER_STRUCT(lvgl_arc_dsc_attrs, lvgl_type_attr_t);

const struct lvgl_dsc_type lvgl_arc_dsc_type = {
    .base = { &lvgl_type_arc_dsc, NULL, NULL, lvgl_dsc_get_handle, lvgl_arc_dsc_attrs },
    .size = sizeof(lv_draw_arc_dsc_t),
    .init = (void *)lv_draw_arc_dsc_init,
    .draw = (void *)lv_draw_arc,
};

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_arc_dsc,
    MP_ROM_QSTR_CONST(MP_QSTR_ArcDsc),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_dsc_make_new,
    attr, lvgl_dsc_attr,
    protocol, &lvgl_arc_dsc_type
    );
MP_REGISTER_OBJECT(lvgl_type_arc_dsc);


static const lvgl_type_attr_t lvgl_image_dsc_attrs[] = {
    { MP_ROM_QSTR_CONST(MP_QSTR_src), offsetof(lv_draw_image_dsc_t, src), LV_TYPE_IMAGE_SRC },
    { MP_ROM_QSTR_CONST(MP_QSTR_rotation), offsetof(lv_draw_image_dsc_t, rotation), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pivot_x), offsetof(lv_draw_image_dsc_t, pivot) + offsetof(lv_point_t, x), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pivot_y), offsetof(lv_draw_image_dsc_t, pivot) + offsetof(lv_point_t, y), LV_TYPE_INT32 },
    { 0 },
 };
MP_REGISTER_STRUCT(lvgl_image_dsc_attrs, lvgl_type_attr_t);

const struct lvgl_dsc_type lvgl_image_dsc_type = {
    .base = { &lvgl_type_image_dsc, NULL, NULL, lvgl_dsc_get_handle, lvgl_image_dsc_attrs },
    .size = sizeof(lv_draw_image_dsc_t),
    .init = (void *)lv_draw_image_dsc_init,
    .draw_coords = (void *)lv_draw_image,
};

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_image_dsc,
    MP_ROM_QSTR_CONST(MP_QSTR_ImageDsc),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_dsc_make_new,
    attr, lvgl_dsc_attr,
    protocol, &lvgl_image_dsc_type
    );
MP_REGISTER_OBJECT(lvgl_type_image_dsc);


static const lvgl_type_attr_t lvgl_label_dsc_attrs[] = {
    { MP_ROM_QSTR_CONST(MP_QSTR_text), offsetof(lv_draw_label_dsc_t, text), LV_TYPE_STR },
    { MP_ROM_QSTR_CONST(MP_QSTR_font), offsetof(lv_draw_label_dsc_t, font), LV_TYPE_FONT },
    { MP_ROM_QSTR_CONST(MP_QSTR_color), offsetof(lv_draw_label_dsc_t, color), LV_TYPE_COLOR },
    { 0 },
 };
MP_REGISTER_STRUCT(lvgl_label_dsc_attrs, lvgl_type_attr_t);

const struct lvgl_dsc_type lvgl_label_dsc_type = {
    .base = { &lvgl_type_label_dsc, NULL, NULL, lvgl_dsc_get_handle, lvgl_label_dsc_attrs },
    .size = sizeof(lv_draw_label_dsc_t),
    .init = (void *)lv_draw_label_dsc_init,
    .draw_coords = (void *)lv_draw_label,
};

static void lvgl_label_dsc_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_dsc_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    lv_draw_label_dsc_t *dsc = (void *)handle->dsc;
    if (attr == MP_QSTR_decor) {
        dsc->decor = lvgl_bitfield_attr_int(attr, dest, dsc->decor);
    }  
    else {
        lvgl_dsc_attr(self_in, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_label_dsc,
    MP_ROM_QSTR_CONST(MP_QSTR_LabelDsc),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_dsc_make_new,
    attr, lvgl_label_dsc_attr,
    protocol, &lvgl_label_dsc_type
    );
MP_REGISTER_OBJECT(lvgl_type_label_dsc);


static const lvgl_type_attr_t lvgl_line_dsc_attrs[] = {
    { MP_ROM_QSTR_CONST(MP_QSTR_p1_x), offsetof(lv_draw_line_dsc_t, p1) + offsetof(lv_point_precise_t, x), LV_TYPE_FLOAT },
    { MP_ROM_QSTR_CONST(MP_QSTR_p1_y), offsetof(lv_draw_line_dsc_t, p1) + offsetof(lv_point_precise_t, y), LV_TYPE_FLOAT },
    { MP_ROM_QSTR_CONST(MP_QSTR_p2_x), offsetof(lv_draw_line_dsc_t, p2) + offsetof(lv_point_precise_t, x), LV_TYPE_FLOAT },
    { MP_ROM_QSTR_CONST(MP_QSTR_p2_y), offsetof(lv_draw_line_dsc_t, p2) + offsetof(lv_point_precise_t, y), LV_TYPE_FLOAT },
    { MP_ROM_QSTR_CONST(MP_QSTR_color), offsetof(lv_draw_line_dsc_t, color), LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_width), offsetof(lv_draw_line_dsc_t, width), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_opa), offsetof(lv_draw_line_dsc_t, opa), LV_TYPE_INT8 },
    { 0 },
 };
MP_REGISTER_STRUCT(lvgl_line_dsc_attrs, lvgl_type_attr_t);

const struct lvgl_dsc_type lvgl_line_dsc_type = {
    .base = { &lvgl_type_line_dsc, NULL, NULL, lvgl_dsc_get_handle, lvgl_line_dsc_attrs },
    .size = sizeof(lv_draw_line_dsc_t),
    .init = (void *)lv_draw_line_dsc_init,
    .draw = (void *)lv_draw_line,
};

static void lvgl_line_dsc_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_dsc_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    lv_draw_line_dsc_t *dsc = (void *)handle->dsc;
    if (attr == MP_QSTR_round_start) {
        dsc->round_start = lvgl_bitfield_attr_bool(attr, dest, dsc->round_start);
    }
    else if (attr == MP_QSTR_round_end) {
        dsc->round_end = lvgl_bitfield_attr_bool(attr, dest, dsc->round_end);
    }    
    else {
        lvgl_dsc_attr(self_in, attr, dest);
    }
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_line_dsc,
    MP_ROM_QSTR_CONST(MP_QSTR_LineDsc),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_dsc_make_new,
    attr, lvgl_line_dsc_attr,
    protocol, &lvgl_line_dsc_type
    );
MP_REGISTER_OBJECT(lvgl_type_line_dsc);


static const lvgl_type_attr_t lvgl_rect_dsc_attrs[] = {
    { MP_ROM_QSTR_CONST(MP_QSTR_radius), offsetof(lv_draw_rect_dsc_t, radius), LV_TYPE_INT32 },

    /*Background*/
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_opa), offsetof(lv_draw_rect_dsc_t, bg_opa), LV_TYPE_INT8 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_color), offsetof(lv_draw_rect_dsc_t, bg_color), LV_TYPE_COLOR },
    // { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad), offsetof(lv_draw_rect_dsc_t, bg_grad), LV_TYPE_INT8 },

    /*Border*/
    { MP_ROM_QSTR_CONST(MP_QSTR_border_color), offsetof(lv_draw_rect_dsc_t, border_color), LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_border_width), offsetof(lv_draw_rect_dsc_t, border_width), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_border_opa), offsetof(lv_draw_rect_dsc_t, border_opa), LV_TYPE_INT8 },

    /*Outline*/
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_color), offsetof(lv_draw_rect_dsc_t, outline_color), LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_width), offsetof(lv_draw_rect_dsc_t, outline_width), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_pad), offsetof(lv_draw_rect_dsc_t, outline_pad), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_opa), offsetof(lv_draw_rect_dsc_t, outline_opa), LV_TYPE_INT8 },

    /*Shadow*/
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_color), offsetof(lv_draw_rect_dsc_t, shadow_color), LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_width), offsetof(lv_draw_rect_dsc_t, shadow_width), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_offset_x), offsetof(lv_draw_rect_dsc_t, shadow_offset_x), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_offset_y), offsetof(lv_draw_rect_dsc_t, shadow_offset_y), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_spread), offsetof(lv_draw_rect_dsc_t, shadow_spread), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_opa), offsetof(lv_draw_rect_dsc_t, shadow_opa), LV_TYPE_INT8 },

    { 0 },
 };
MP_REGISTER_STRUCT(lvgl_rect_dsc_attrs, lvgl_type_attr_t);

const struct lvgl_dsc_type lvgl_rect_dsc_type = {
    .base = { &lvgl_type_rect_dsc, NULL, NULL, lvgl_dsc_get_handle, lvgl_rect_dsc_attrs },
    .size = sizeof(lv_draw_rect_dsc_t),
    .init = (void *)lv_draw_rect_dsc_init,
    .draw_coords = (void *)lv_draw_rect,
};

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_rect_dsc,
    MP_ROM_QSTR_CONST(MP_QSTR_RectDsc),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_dsc_make_new,
    attr, lvgl_dsc_attr,
    protocol, &lvgl_rect_dsc_type
    );
MP_REGISTER_OBJECT(lvgl_type_rect_dsc);
