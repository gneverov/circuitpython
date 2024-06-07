// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./color.h"
#include "./types.h"


static mp_obj_t lvgl_palette_main(mp_obj_t self_in) {
    lvgl_obj_palette_t *self = MP_OBJ_TO_PTR(self_in);
    lv_color_t c = lv_palette_main(self->p);
    return mp_obj_new_int(lv_color_to_int(c));
}
static MP_DEFINE_CONST_FUN_OBJ_1(lvgl_palette_main_obj, lvgl_palette_main);

static mp_obj_t lvgl_palette_lighten(mp_obj_t self_in, mp_obj_t lvl_in) {
    lvgl_obj_palette_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t lvl = mp_obj_get_int(lvl_in);
    lv_color_t c = lv_palette_lighten(self->p, lvl);
    return mp_obj_new_int(lv_color_to_int(c));
}
static MP_DEFINE_CONST_FUN_OBJ_2(lvgl_palette_lighten_obj, lvgl_palette_lighten);

static mp_obj_t lvgl_palette_darken(mp_obj_t self_in, mp_obj_t lvl_in) {
    lvgl_obj_palette_t *self = MP_OBJ_TO_PTR(self_in);
    uint8_t lvl = mp_obj_get_int(lvl_in);
    lv_color_t c = lv_palette_darken(self->p, lvl);
    return mp_obj_new_int(lv_color_to_int(c));
}
static MP_DEFINE_CONST_FUN_OBJ_2(lvgl_palette_darken_obj, lvgl_palette_darken);

static const lvgl_obj_palette_t lvgl_palettes[] = {
    { { &lvgl_type_palette }, LV_PALETTE_RED },
    { { &lvgl_type_palette }, LV_PALETTE_PINK },
    { { &lvgl_type_palette }, LV_PALETTE_PURPLE },
    { { &lvgl_type_palette }, LV_PALETTE_DEEP_PURPLE },
    { { &lvgl_type_palette }, LV_PALETTE_INDIGO },
    { { &lvgl_type_palette }, LV_PALETTE_BLUE },
    { { &lvgl_type_palette }, LV_PALETTE_LIGHT_BLUE },
    { { &lvgl_type_palette }, LV_PALETTE_CYAN },
    { { &lvgl_type_palette }, LV_PALETTE_TEAL },
    { { &lvgl_type_palette }, LV_PALETTE_GREEN },
    { { &lvgl_type_palette }, LV_PALETTE_LIGHT_GREEN },
    { { &lvgl_type_palette }, LV_PALETTE_LIME },
    { { &lvgl_type_palette }, LV_PALETTE_YELLOW },
    { { &lvgl_type_palette }, LV_PALETTE_AMBER },
    { { &lvgl_type_palette }, LV_PALETTE_ORANGE },
    { { &lvgl_type_palette }, LV_PALETTE_DEEP_ORANGE },
    { { &lvgl_type_palette }, LV_PALETTE_BROWN },
    { { &lvgl_type_palette }, LV_PALETTE_BLUE_GREY },
    { { &lvgl_type_palette }, LV_PALETTE_GREY },
};

static const mp_rom_map_elem_t lvgl_palette_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_main),            MP_ROM_PTR(&lvgl_palette_main_obj) },
    { MP_ROM_QSTR(MP_QSTR_lighten),         MP_ROM_PTR(&lvgl_palette_lighten_obj) },
    { MP_ROM_QSTR(MP_QSTR_darken),          MP_ROM_PTR(&lvgl_palette_darken_obj) },

    { MP_ROM_QSTR(MP_QSTR_RED),             MP_ROM_PTR(&lvgl_palettes[0]) },
    { MP_ROM_QSTR(MP_QSTR_PINK),            MP_ROM_PTR(&lvgl_palettes[1]) },
    { MP_ROM_QSTR(MP_QSTR_PURPLE),          MP_ROM_PTR(&lvgl_palettes[2]) },
    { MP_ROM_QSTR(MP_QSTR_DEEP_PURPLE),     MP_ROM_PTR(&lvgl_palettes[3]) },
    { MP_ROM_QSTR(MP_QSTR_INDIGO),          MP_ROM_PTR(&lvgl_palettes[4]) },
    { MP_ROM_QSTR(MP_QSTR_BLUE),            MP_ROM_PTR(&lvgl_palettes[5]) },
    { MP_ROM_QSTR(MP_QSTR_LIGHT_BLUE),      MP_ROM_PTR(&lvgl_palettes[6]) },
    { MP_ROM_QSTR(MP_QSTR_CYAN),            MP_ROM_PTR(&lvgl_palettes[7]) },
    { MP_ROM_QSTR(MP_QSTR_TEAL),            MP_ROM_PTR(&lvgl_palettes[8]) },
    { MP_ROM_QSTR(MP_QSTR_GREEN),           MP_ROM_PTR(&lvgl_palettes[9]) },
    { MP_ROM_QSTR(MP_QSTR_LIGHT_GREEN),     MP_ROM_PTR(&lvgl_palettes[10]) },
    { MP_ROM_QSTR(MP_QSTR_LIME),            MP_ROM_PTR(&lvgl_palettes[11]) },
    { MP_ROM_QSTR(MP_QSTR_YELLOW),          MP_ROM_PTR(&lvgl_palettes[12]) },
    { MP_ROM_QSTR(MP_QSTR_AMBER),           MP_ROM_PTR(&lvgl_palettes[13]) },
    { MP_ROM_QSTR(MP_QSTR_ORANGE),          MP_ROM_PTR(&lvgl_palettes[14]) },
    { MP_ROM_QSTR(MP_QSTR_DEEP_ORANGE),     MP_ROM_PTR(&lvgl_palettes[15]) },
    { MP_ROM_QSTR(MP_QSTR_BROWN),           MP_ROM_PTR(&lvgl_palettes[16]) },
    { MP_ROM_QSTR(MP_QSTR_BLUE_GREY),       MP_ROM_PTR(&lvgl_palettes[17]) },
    { MP_ROM_QSTR(MP_QSTR_GREY),            MP_ROM_PTR(&lvgl_palettes[18]) },
};
static MP_DEFINE_CONST_DICT(lvgl_palette_locals_dict, lvgl_palette_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_palette,
    MP_ROM_QSTR_CONST(MP_QSTR_Palette),
    MP_TYPE_FLAG_NONE,
    locals_dict, &lvgl_palette_locals_dict 
    );
MP_REGISTER_OBJECT(lvgl_type_palette);


typedef lvgl_obj_static_ptr_t lvgl_obj_color_filter_t;

static lv_color_t lvgl_color_filter_cb(const lv_color_filter_dsc_t *color_filter, lv_color_t c, lv_opa_t lvl) {
    lv_color_t (*cb)(lv_color_t, lv_opa_t) = color_filter->user_data;
    return cb(c, lvl);
}

static const lv_color_filter_dsc_t lv_color_filters[] = {
    { lvgl_color_filter_cb, lv_color_darken },
    { lvgl_color_filter_cb, lv_color_lighten },
};

static const lvgl_obj_color_filter_t lvgl_color_filters[] = {
    { { &lvgl_type_color_filter }, &lv_color_filters[0] },
    { { &lvgl_type_color_filter }, &lv_color_filters[1] },
    { { &lvgl_type_color_filter }, &lv_color_filter_shade },
};

static mp_obj_t lvgl_color_filter_call(mp_obj_t self_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 3, 3, false);
    lvgl_obj_color_filter_t *self = MP_OBJ_TO_PTR(args[0]);
    const lv_color_filter_dsc_t *color_filter = self->lv_ptr;
    lv_color_t c = lv_color_hex(mp_obj_get_int(args[1]));
    lv_opa_t lvl = mp_obj_get_int(args[2]);
    c = color_filter->filter_cb(color_filter, c, lvl);
    return mp_obj_new_int(lv_color_to_int(c));
}

static const mp_rom_map_elem_t lvgl_color_filter_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_DARKEN),          MP_ROM_PTR(&lvgl_color_filters[0]) },
    { MP_ROM_QSTR(MP_QSTR_LIGHTEN),         MP_ROM_PTR(&lvgl_color_filters[1]) },
    { MP_ROM_QSTR(MP_QSTR_SHADE),           MP_ROM_PTR(&lvgl_color_filters[2]) },
};
static MP_DEFINE_CONST_DICT(lvgl_color_filter_locals_dict, lvgl_color_filter_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_color_filter,
    MP_ROM_QSTR_CONST(MP_QSTR_ColorFilter),
    MP_TYPE_FLAG_NONE,
    call, &lvgl_color_filter_call,
    locals_dict, &lvgl_color_filter_locals_dict 
    );
MP_REGISTER_OBJECT(lvgl_type_color_filter);

const lvgl_static_ptr_type_t lvgl_color_filter_type = {
    &lvgl_type_color_filter,
    &lvgl_color_filter_locals_dict.map,
};
