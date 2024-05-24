// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./font.h"
#include "./super.h"
#include "./types.h"


typedef lvgl_obj_static_ptr_t lvgl_obj_font_t;

STATIC void lvgl_font_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    const lvgl_obj_font_t *self = MP_OBJ_TO_PTR(self_in);
    const lv_font_t *font = self->lv_ptr;
    if (attr == MP_QSTR_line_height) {
        lvgl_super_attr_check(attr, true, false, false, dest);
        dest[0] = mp_obj_new_int(lv_font_get_line_height(font));
    }
    else {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

#if LV_FONT_MONTSERRAT_8
STATIC const lvgl_obj_font_t lvgl_font_montserrat_8 = { { &lvgl_type_font }, &lv_font_montserrat_8 };
#endif

#if LV_FONT_MONTSERRAT_10
STATIC const lvgl_obj_font_t lvgl_font_montserrat_10 = { { &lvgl_type_font }, &lv_font_montserrat_10 };
#endif

#if LV_FONT_MONTSERRAT_12
STATIC const lvgl_obj_font_t lvgl_font_montserrat_12 = { { &lvgl_type_font }, &lv_font_montserrat_12 };
#endif

#if LV_FONT_MONTSERRAT_14
STATIC const lvgl_obj_font_t lvgl_font_montserrat_14 = { { &lvgl_type_font }, &lv_font_montserrat_14 };
#endif

#if LV_FONT_MONTSERRAT_16
STATIC const lvgl_obj_font_t lvgl_font_montserrat_16 = { { &lvgl_type_font }, &lv_font_montserrat_16 };
#endif

#if LV_FONT_MONTSERRAT_18
STATIC const lvgl_obj_font_t lvgl_font_montserrat_18 = { { &lvgl_type_font }, &lv_font_montserrat_18 };
#endif

#if LV_FONT_MONTSERRAT_20
STATIC const lvgl_obj_font_t lvgl_font_montserrat_20 = { { &lvgl_type_font }, &lv_font_montserrat_20 };
#endif

#if LV_FONT_MONTSERRAT_22
STATIC const lvgl_obj_font_t lvgl_font_montserrat_22 = { { &lvgl_type_font }, &lv_font_montserrat_22 };
#endif

#if LV_FONT_MONTSERRAT_24
STATIC const lvgl_obj_font_t lvgl_font_montserrat_24 = { { &lvgl_type_font }, &lv_font_montserrat_24 };
#endif

#if LV_FONT_MONTSERRAT_26
STATIC const lvgl_obj_font_t lvgl_font_montserrat_26 = { { &lvgl_type_font }, &lv_font_montserrat_26 };
#endif

#if LV_FONT_MONTSERRAT_28
STATIC const lvgl_obj_font_t lvgl_font_montserrat_28 = { { &lvgl_type_font }, &lv_font_montserrat_28 };
#endif

#if LV_FONT_MONTSERRAT_30
STATIC const lvgl_obj_font_t lvgl_font_montserrat_30 = { { &lvgl_type_font }, &lv_font_montserrat_30 };
#endif

#if LV_FONT_MONTSERRAT_32
STATIC const lvgl_obj_font_t lvgl_font_montserrat_32 = { { &lvgl_type_font }, &lv_font_montserrat_32 };
#endif

#if LV_FONT_MONTSERRAT_34
STATIC const lvgl_obj_font_t lvgl_font_montserrat_34 = { { &lvgl_type_font }, &lv_font_montserrat_34 };
#endif

#if LV_FONT_MONTSERRAT_36
STATIC const lvgl_obj_font_t lvgl_font_montserrat_36 = { { &lvgl_type_font }, &lv_font_montserrat_36 };
#endif

#if LV_FONT_MONTSERRAT_38
STATIC const lvgl_obj_font_t lvgl_font_montserrat_38 = { { &lvgl_type_font }, &lv_font_montserrat_38 };
#endif

#if LV_FONT_MONTSERRAT_40
STATIC const lvgl_obj_font_t lvgl_font_montserrat_40 = { { &lvgl_type_font }, &lv_font_montserrat_40 };
#endif

#if LV_FONT_MONTSERRAT_42
STATIC const lvgl_obj_font_t lvgl_font_montserrat_42 = { { &lvgl_type_font }, &lv_font_montserrat_42 };
#endif

#if LV_FONT_MONTSERRAT_44
STATIC const lvgl_obj_font_t lvgl_font_montserrat_44 = { { &lvgl_type_font }, &lv_font_montserrat_44 };
#endif

#if LV_FONT_MONTSERRAT_46
STATIC const lvgl_obj_font_t lvgl_font_montserrat_46 = { { &lvgl_type_font }, &lv_font_montserrat_46 };
#endif

#if LV_FONT_MONTSERRAT_48
STATIC const lvgl_obj_font_t lvgl_font_montserrat_48 = { { &lvgl_type_font }, &lv_font_montserrat_48 };
#endif

#if LV_FONT_MONTSERRAT_28_COMPRESSED
STATIC const lvgl_obj_font_t lvgl_font_montserrat_28_compressed = { { &lvgl_type_font }, &lv_font_montserrat_28_compressed };
#endif

#if LV_FONT_DEJAVU_16_PERSIAN_HEBREW
STATIC const lvgl_obj_font_t lvgl_font_dejavu_16_persian_hebrew = { { &lvgl_type_font }, &lv_font_dejavu_16_persian_hebrew };
#endif

#if LV_FONT_SIMSUN_16_CJK
STATIC const lvgl_obj_font_t lvgl_font_simsun_16_cjk = { { &lvgl_type_font }, &lv_font_simsun_16_cjk };
#endif

#if LV_FONT_UNSCII_8
STATIC const lvgl_obj_font_t lvgl_font_unscii_8 = { { &lvgl_type_font }, &lv_font_unscii_8 };
#endif

#if LV_FONT_UNSCII_16
STATIC const lvgl_obj_font_t lvgl_font_unscii_16 = { { &lvgl_type_font }, &lv_font_unscii_16 };
#endif

static_assert(LV_FONT_DEFAULT == &lv_font_montserrat_14);

STATIC const mp_rom_map_elem_t lvgl_font_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_default),             MP_ROM_PTR(&lvgl_font_montserrat_14) },

#if LV_FONT_MONTSERRAT_8
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_8),        MP_ROM_PTR(&lvgl_font_montserrat_8) },
#endif

#if LV_FONT_MONTSERRAT_10
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_10),       MP_ROM_PTR(&lvgl_font_montserrat_10) },
#endif

#if LV_FONT_MONTSERRAT_12
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_12),       MP_ROM_PTR(&lvgl_font_montserrat_12) },
#endif

#if LV_FONT_MONTSERRAT_14
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_14),       MP_ROM_PTR(&lvgl_font_montserrat_14) },
#endif

#if LV_FONT_MONTSERRAT_16
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_16),       MP_ROM_PTR(&lvgl_font_montserrat_16) },
#endif

#if LV_FONT_MONTSERRAT_18
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_18),       MP_ROM_PTR(&lvgl_font_montserrat_18) },
#endif

#if LV_FONT_MONTSERRAT_20
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_20),       MP_ROM_PTR(&lvgl_font_montserrat_20) },
#endif

#if LV_FONT_MONTSERRAT_22
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_22),       MP_ROM_PTR(&lvgl_font_montserrat_22) },
#endif

#if LV_FONT_MONTSERRAT_24
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_24),       MP_ROM_PTR(&lvgl_font_montserrat_24) },
#endif

#if LV_FONT_MONTSERRAT_26
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_26),       MP_ROM_PTR(&lvgl_font_montserrat_26) },
#endif

#if LV_FONT_MONTSERRAT_28
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_28),       MP_ROM_PTR(&lvgl_font_montserrat_28) },
#endif

#if LV_FONT_MONTSERRAT_30
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_30),       MP_ROM_PTR(&lvgl_font_montserrat_30) },
#endif

#if LV_FONT_MONTSERRAT_32
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_32),       MP_ROM_PTR(&lvgl_font_montserrat_32) },
#endif

#if LV_FONT_MONTSERRAT_34
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_34),       MP_ROM_PTR(&lvgl_font_montserrat_34) },
#endif

#if LV_FONT_MONTSERRAT_36
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_36),       MP_ROM_PTR(&lvgl_font_montserrat_36) },
#endif

#if LV_FONT_MONTSERRAT_38
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_38),       MP_ROM_PTR(&lvgl_font_montserrat_38) },
#endif

#if LV_FONT_MONTSERRAT_40
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_40),       MP_ROM_PTR(&lvgl_font_montserrat_40) },
#endif

#if LV_FONT_MONTSERRAT_42
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_42),       MP_ROM_PTR(&lvgl_font_montserrat_42) },
#endif

#if LV_FONT_MONTSERRAT_44
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_44),       MP_ROM_PTR(&lvgl_font_montserrat_44) },
#endif

#if LV_FONT_MONTSERRAT_46
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_46),       MP_ROM_PTR(&lvgl_font_montserrat_46) },
#endif

#if LV_FONT_MONTSERRAT_48
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_48),       MP_ROM_PTR(&lvgl_font_montserrat_48) },
#endif

#if LV_FONT_MONTSERRAT_28_COMPRESSED
    { MP_ROM_QSTR(MP_QSTR_MONTSERRAT_28_COMPRESSED), MP_ROM_PTR(&lvgl_font_montserrat_28_compressed) },
#endif

#if LV_FONT_DEJAVU_16_PERSIAN_HEBREW
    { MP_ROM_QSTR(MP_QSTR_DEJAVU_16_PERSIAN_HEBREW), MP_ROM_PTR(&lvgl_font_dejavu_16_persian_hebrew) },
#endif

#if LV_FONT_SIMSUN_16_CJK
    { MP_ROM_QSTR(MP_QSTR_SIMSUN_16_CJK),       MP_ROM_PTR(&lvgl_font_simsun_16_cjk) },
#endif

#if LV_FONT_UNSCII_8
    { MP_ROM_QSTR(MP_QSTR_UNSCII_8),            MP_ROM_PTR(&lvgl_font_unscii_8) },
#endif

#if LV_FONT_UNSCII_16
    { MP_ROM_QSTR(MP_QSTR_UNSCII_16),           MP_ROM_PTR(&lvgl_font_unscii_16) },
#endif
};
STATIC MP_DEFINE_CONST_DICT(lvgl_font_locals_dict, lvgl_font_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_font,
    MP_ROM_QSTR_CONST(MP_QSTR_Font),
    MP_TYPE_FLAG_NONE,
    attr, lvgl_font_attr,
    locals_dict, &lvgl_font_locals_dict 
    );
MP_REGISTER_OBJECT(lvgl_type_font);

const lvgl_static_ptr_type_t lvgl_font_type = {
    &lvgl_type_font,
    &lvgl_font_locals_dict.map,
};
