// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./buffer.h"
#include "./draw.h"
#include "./image_decoder.h"
#include "./layer.h"
#include "./moddraw.h"


static const mp_rom_map_elem_t lvgl_module_draw_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_lvgl_dot_draw) },

    { MP_ROM_QSTR(MP_QSTR_Buffer),          MP_ROM_PTR(&lvgl_type_draw_buf) },
    { MP_ROM_QSTR(MP_QSTR_ImageDecoder),    MP_ROM_PTR(&lvgl_type_image_decoder) },
    { MP_ROM_QSTR(MP_QSTR_Layer),           MP_ROM_PTR(&lvgl_type_layer) },

    { MP_ROM_QSTR(MP_QSTR_ArcDsc),          MP_ROM_PTR(&lvgl_type_arc_dsc) },
    { MP_ROM_QSTR(MP_QSTR_ImageDsc),        MP_ROM_PTR(&lvgl_type_image_dsc) },
    { MP_ROM_QSTR(MP_QSTR_LabelDsc),        MP_ROM_PTR(&lvgl_type_label_dsc) },
    { MP_ROM_QSTR(MP_QSTR_LineDsc),         MP_ROM_PTR(&lvgl_type_line_dsc) },
    { MP_ROM_QSTR(MP_QSTR_RectDsc),         MP_ROM_PTR(&lvgl_type_rect_dsc) },    
};
static MP_DEFINE_CONST_DICT(lvgl_module_draw_globals, lvgl_module_draw_globals_table);

const mp_obj_module_t lvgl_module_draw = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&lvgl_module_draw_globals,
};

MP_REGISTER_MODULE(MP_QSTR_lvgl_dot_draw, lvgl_module_draw);
MP_REGISTER_OBJECT(lvgl_module_draw);
