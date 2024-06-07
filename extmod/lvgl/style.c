// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <stdlib.h>

#include "extmod/freeze/extmod.h"
#include "./anim.h"
#include "./color.h"
#include "./font.h"
#include "./modlvgl.h"
#include "./style.h"
#include "./super.h"
#include "./types.h"

#include "py/runtime.h"


static struct lvgl_style {
    qstr_short_t qstr;
    lv_style_prop_t prop;
    lv_type_code_t type_code;
} lvgl_style_table[] = {
    /*Group 0*/
    { MP_ROM_QSTR_CONST(MP_QSTR_width), LV_STYLE_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_height), LV_STYLE_HEIGHT, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_min_width), LV_STYLE_MIN_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_max_width), LV_STYLE_MAX_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_min_height), LV_STYLE_MIN_HEIGHT, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_max_height), LV_STYLE_MAX_HEIGHT, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_x), LV_STYLE_X, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_y), LV_STYLE_Y, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_align), LV_STYLE_ALIGN, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_radius), LV_STYLE_RADIUS, LV_TYPE_INT32 },

    /*Group 1*/
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_top), LV_STYLE_PAD_TOP, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_bottom), LV_STYLE_PAD_BOTTOM, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_left), LV_STYLE_PAD_LEFT, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_right), LV_STYLE_PAD_RIGHT, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_pad_row), LV_STYLE_PAD_ROW, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_column), LV_STYLE_PAD_COLUMN, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_layout), LV_STYLE_LAYOUT, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_margin_top), LV_STYLE_MARGIN_TOP, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_margin_bottom), LV_STYLE_MARGIN_BOTTOM, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_margin_left), LV_STYLE_MARGIN_LEFT, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_margin_right), LV_STYLE_MARGIN_RIGHT, LV_TYPE_INT32 },

    /*Group 2*/
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_color), LV_STYLE_BG_COLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_opa), LV_STYLE_BG_OPA, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad_dir), LV_STYLE_BG_GRAD_DIR, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_main_stop), LV_STYLE_BG_MAIN_STOP, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad_stop), LV_STYLE_BG_GRAD_STOP, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad_color), LV_STYLE_BG_GRAD_COLOR, LV_TYPE_COLOR },

    { MP_ROM_QSTR_CONST(MP_QSTR_bg_main_opa), LV_STYLE_BG_MAIN_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad_opa), LV_STYLE_BG_GRAD_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad), LV_STYLE_BG_GRAD, LV_TYPE_GRAD_DSC },
    { MP_ROM_QSTR_CONST(MP_QSTR_base_dir), LV_STYLE_BASE_DIR, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_src), LV_STYLE_BG_IMAGE_SRC, LV_TYPE_IMAGE_SRC },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_opa), LV_STYLE_BG_IMAGE_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_recolor), LV_STYLE_BG_IMAGE_RECOLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_recolor_opa), LV_STYLE_BG_IMAGE_RECOLOR_OPA, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_tiled), LV_STYLE_BG_IMAGE_TILED, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_clip_corner), LV_STYLE_CLIP_CORNER, LV_TYPE_INT32 },

    /*Group 3*/
    { MP_ROM_QSTR_CONST(MP_QSTR_border_width), LV_STYLE_BORDER_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_border_color), LV_STYLE_BORDER_COLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_border_opa), LV_STYLE_BORDER_OPA, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_border_side), LV_STYLE_BORDER_SIDE, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_border_post), LV_STYLE_BORDER_POST, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_outline_width), LV_STYLE_OUTLINE_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_color), LV_STYLE_OUTLINE_COLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_opa), LV_STYLE_OUTLINE_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_pad), LV_STYLE_OUTLINE_PAD, LV_TYPE_INT32 },

    /*Group 4*/
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_width), LV_STYLE_SHADOW_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_color), LV_STYLE_SHADOW_COLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_opa), LV_STYLE_SHADOW_OPA, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_offset_x), LV_STYLE_SHADOW_OFFSET_X, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_offset_y), LV_STYLE_SHADOW_OFFSET_Y, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_shadow_spread), LV_STYLE_SHADOW_SPREAD, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_image_opa), LV_STYLE_IMAGE_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_image_recolor), LV_STYLE_IMAGE_RECOLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_image_recolor_opa), LV_STYLE_IMAGE_RECOLOR_OPA, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_line_width), LV_STYLE_LINE_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_dash_width), LV_STYLE_LINE_DASH_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_dash_gap), LV_STYLE_LINE_DASH_GAP, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_rounded), LV_STYLE_LINE_ROUNDED, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_color), LV_STYLE_LINE_COLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_opa), LV_STYLE_LINE_OPA, LV_TYPE_INT32 },

    /*Group 5*/
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_width), LV_STYLE_ARC_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_rounded), LV_STYLE_ARC_ROUNDED, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_color), LV_STYLE_ARC_COLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_opa), LV_STYLE_ARC_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_image_src), LV_STYLE_ARC_IMAGE_SRC, LV_TYPE_NONE },

    { MP_ROM_QSTR_CONST(MP_QSTR_text_color), LV_STYLE_TEXT_COLOR, LV_TYPE_COLOR },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_opa), LV_STYLE_TEXT_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_font), LV_STYLE_TEXT_FONT, LV_TYPE_FONT },

    { MP_ROM_QSTR_CONST(MP_QSTR_text_letter_space), LV_STYLE_TEXT_LETTER_SPACE, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_line_space), LV_STYLE_TEXT_LINE_SPACE, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_decor), LV_STYLE_TEXT_DECOR, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_align), LV_STYLE_TEXT_ALIGN, LV_TYPE_INT32 },

    { MP_ROM_QSTR_CONST(MP_QSTR_opa), LV_STYLE_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_opa_layered), LV_STYLE_OPA_LAYERED, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_color_filter_dsc), LV_STYLE_COLOR_FILTER_DSC, LV_TYPE_COLOR_FILTER },
    { MP_ROM_QSTR_CONST(MP_QSTR_color_filter_opa), LV_STYLE_COLOR_FILTER_OPA, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_anim), LV_STYLE_ANIM, LV_TYPE_ANIM },
    { MP_ROM_QSTR_CONST(MP_QSTR_anim_duration), LV_STYLE_ANIM_DURATION, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transition), LV_STYLE_TRANSITION, LV_TYPE_STYLE_TRANSITION_DSC },
    { MP_ROM_QSTR_CONST(MP_QSTR_blend_mode), LV_STYLE_BLEND_MODE, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_width), LV_STYLE_TRANSFORM_WIDTH, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_height), LV_STYLE_TRANSFORM_HEIGHT, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_translate_x), LV_STYLE_TRANSLATE_X, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_translate_y), LV_STYLE_TRANSLATE_Y, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_scale_x), LV_STYLE_TRANSFORM_SCALE_X, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_scale_y), LV_STYLE_TRANSFORM_SCALE_Y, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_rotation), LV_STYLE_TRANSFORM_ROTATION, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_pivot_x), LV_STYLE_TRANSFORM_PIVOT_X, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_pivot_y), LV_STYLE_TRANSFORM_PIVOT_Y, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_skew_x), LV_STYLE_TRANSFORM_SKEW_X, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_transform_skew_y), LV_STYLE_TRANSFORM_SKEW_Y, LV_TYPE_INT32 }, 
    // { MP_ROM_QSTR_CONST(MP_QSTR_bitmap_mask_src), LV_STYLE_BITMAP_MASK_SRC, LV_TYPE_NONE }, 
    { MP_ROM_QSTR_CONST(MP_QSTR_rotary_sensitivity), LV_STYLE_ROTARY_SENSITIVITY, LV_TYPE_INT32 }, 

#if LV_USE_FLEX
    { MP_ROM_QSTR_CONST(MP_QSTR_flex_flow), LV_STYLE_FLEX_FLOW, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_flex_main_place), LV_STYLE_FLEX_MAIN_PLACE, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_flex_cross_place), LV_STYLE_FLEX_CROSS_PLACE, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_flex_track_place), LV_STYLE_FLEX_TRACK_PLACE, LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_flex_grow), LV_STYLE_FLEX_GROW, LV_TYPE_INT32 },
#endif

#if LV_USE_GRID
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_column_align), LV_STYLE_GRID_COLUMN_ALIGN, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_row_align), LV_STYLE_GRID_ROW_ALIGN, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_row_dsc_array), LV_STYLE_GRID_ROW_DSC_ARRAY, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_column_dsc_array), LV_STYLE_GRID_COLUMN_DSC_ARRAY, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_cell_column_pos), LV_STYLE_GRID_CELL_COLUMN_POS, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_cell_column_span), LV_STYLE_GRID_CELL_COLUMN_SPAN, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_x_align), LV_STYLE_GRID_CELL_X_ALIGN, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_row_pos), LV_STYLE_GRID_CELL_ROW_POS, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_row_span), LV_STYLE_GRID_CELL_ROW_SPAN, LV_TYPE_NONE },
//     { MP_ROM_QSTR_CONST(MP_QSTR_grid_y_align), LV_STYLE_GRID_CELL_Y_ALIGN, LV_TYPE_NONE },
#endif    
};

static lv_style_prop_t lvgl_handle_prop;

static int lvgl_style_compare(const void *lhs, const void *rhs) {
    const struct lvgl_style *lhs_style = lhs;
    const struct lvgl_style *rhs_style = rhs;
    return lhs_style->qstr - rhs_style->qstr;
}

void lvgl_style_init(void) {
    size_t num_styles = sizeof(lvgl_style_table) / sizeof(struct lvgl_style);
#if MICROPY_PY_EXTENSION
    for (int i = 0; i < num_styles; i++) {
        mp_extmod_qstr(mp_extmod_qstr_table, MP_EXTMOD_NUM_QSTRS, &lvgl_style_table[i].qstr);
    }
#endif
    qsort(lvgl_style_table, num_styles, sizeof(struct lvgl_style), lvgl_style_compare);

    lvgl_handle_prop = lv_style_register_prop(0);
}

lv_style_prop_t lvgl_style_lookup(qstr qstr, lv_type_code_t *type_code) {
    struct lvgl_style key = { qstr, 0, 0 };
    struct lvgl_style *value = bsearch(&key, lvgl_style_table, sizeof(lvgl_style_table) / sizeof(struct lvgl_style), sizeof(struct lvgl_style), lvgl_style_compare);
    if (value->qstr != key.qstr) {
        return 0;
    }
    *type_code = value->type_code;
    return value->prop;
}

void lvgl_style_value_free(lv_type_code_t type_code, lv_style_value_t value) {
    lvgl_type_free(type_code, &value.ptr);
}

lv_style_value_t lvgl_style_value_from_mp(lv_type_code_t type_code, mp_obj_t obj) {
    lv_style_value_t value = { 0 };
    lvgl_type_from_mp(type_code, obj, &value.ptr);
    return value;
}

mp_obj_t lvgl_style_value_to_mp(lv_type_code_t type_code, lv_style_value_t value) {
    return lvgl_type_to_mp(type_code, &value.ptr);
}

static lv_style_value_t lvgl_style_value_clone(lv_type_code_t type_code, lv_style_value_t src) {
    lv_style_value_t dst = { 0 };
    lvgl_type_clone(type_code, &dst.ptr, &src.ptr);
    return dst;
}

static lvgl_ptr_t lvgl_style_get_handle0(const void *lv_ptr) {
    const lv_style_t *style = lv_ptr;
    lvgl_style_handle_t *handle = NULL;
    bool is_locked = lvgl_is_locked();
    if (!is_locked) {
        lvgl_lock();
    }
    lv_style_value_t tmp;
    if (lv_style_get_prop(style, lvgl_handle_prop, &tmp) == LV_RESULT_OK) {
        handle = (void *)tmp.ptr;
        assert(&handle->style == style);
    }
    if (!is_locked) {
        lvgl_unlock();
    }
    return handle;
}

lvgl_style_handle_t *lvgl_style_from_mp(mp_obj_t self_in) {
    return lvgl_ptr_from_mp(&lvgl_style_type, self_in);
}

static mp_obj_t lvgl_style_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, true);

    lvgl_style_handle_t *handle = malloc(sizeof(lvgl_style_handle_t));
    lvgl_ptr_init_handle(&handle->base, &lvgl_style_type, &handle->style);
    lv_style_init(&handle->style);

    lvgl_lock();
    lv_style_value_t value = { .ptr = handle };
    lv_style_set_prop(&handle->style, lvgl_handle_prop, value);
    lvgl_unlock();

    mp_obj_t self_out = lvgl_ptr_to_mp(&handle->base);
    lvgl_super_update(self_out, n_kw, (const mp_map_elem_t *)(args + n_args));
    return self_out;
}

void lvgl_style_deinit(lvgl_ptr_t ptr) {
    lvgl_style_handle_t *handle = ptr;
    size_t num_styles = sizeof(lvgl_style_table) / sizeof(struct lvgl_style);
    for (size_t i = 0; i < num_styles; i++) {
        lv_style_value_t tmp;
        if (lv_style_get_prop(&handle->style, lvgl_style_table[i].prop, &tmp) == LV_RESULT_OK) {
            lvgl_style_value_free(lvgl_style_table[i].type_code, tmp);
        }
    }
    lv_style_reset(&handle->style);
}

static void lvgl_style_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lv_type_code_t type_code;
    lv_style_prop_t prop = lvgl_style_lookup(attr, &type_code);
    if (!prop) {
        dest[1] = MP_OBJ_SENTINEL;
        return;
    }

    lv_style_value_t new_value;
    if (dest[1] != MP_OBJ_NULL) {
        new_value = lvgl_style_value_from_mp(type_code, dest[1]);
    }

    lvgl_style_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);
    lvgl_lock();
    lv_style_value_t old_value = { .ptr = NULL };
    bool has_old_value = lv_style_get_prop(&handle->style, prop, &old_value) == LV_RESULT_OK;
    if (dest[0] != MP_OBJ_SENTINEL) {
        if (has_old_value) {
            lv_style_value_t tmp = lvgl_style_value_clone(type_code, old_value);        
            lvgl_unlock();
            dest[0] = lvgl_style_value_to_mp(type_code, tmp);
            lvgl_style_value_free(type_code, tmp);
        }
        else {
            lvgl_unlock();
        }
    }
    else if (dest[1] != MP_OBJ_NULL) {
        lv_style_set_prop(&handle->style, prop, new_value);
        lvgl_unlock();
        if (has_old_value) {
            lvgl_style_value_free(type_code, old_value);
        }        
        dest[0] = MP_OBJ_NULL;
    }
    else {
        bool removed = lv_style_remove_prop(&handle->style, prop);
        lvgl_unlock();
        if (removed) {
            if (has_old_value) {
                lvgl_style_value_free(type_code, old_value);
            }             
            dest[0] = MP_OBJ_NULL;
        }
    }
}

static const mp_rom_map_elem_t lvgl_style_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_ptr_del_obj) },
};
static MP_DEFINE_CONST_DICT(lvgl_style_locals_dict, lvgl_style_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_style,
    MP_ROM_QSTR_CONST(MP_QSTR_Style),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_style_make_new,
    attr, lvgl_style_attr,
    locals_dict, &lvgl_style_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_style);

const lvgl_ptr_type_t lvgl_style_type = {
    &lvgl_type_style,
    NULL,
    lvgl_style_deinit,
    lvgl_style_get_handle0,
    NULL,
};


typedef struct lvgl_grad_dsc_handle {
    lvgl_ptr_handle_t base;
    lv_grad_dsc_t dsc;
} lvgl_grad_dsc_handle_t;

static mp_obj_t lvgl_grad_dsc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, true);
    
    lvgl_grad_dsc_handle_t *handle = lv_malloc_zeroed(sizeof(lvgl_grad_dsc_handle_t));
    lvgl_ptr_init_handle(&handle->base, &lvgl_grad_dsc_type, &handle->dsc);
    mp_obj_t self_out = lvgl_ptr_to_mp(&handle->base);

    lvgl_super_update(self_out, n_kw, (mp_map_elem_t *)(args + n_args));
    return self_out;
}

static const qstr lvgl_gradient_stop_attrs[] = { MP_ROM_QSTR_CONST(MP_QSTR_color), MP_ROM_QSTR_CONST(MP_QSTR_opa), MP_ROM_QSTR_CONST(MP_QSTR_frac) };
MP_REGISTER_STRUCT(lvgl_gradient_stop_attrs, qstr);

static void lvgl_grad_dsc_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    lvgl_grad_dsc_handle_t *handle = lvgl_ptr_from_mp(NULL, self_in);

    if (attr == MP_QSTR_dir) {
        handle->dsc.dir = lvgl_bitfield_attr_int(attr, dest, handle->dsc.dir);
    }
    else if (attr == MP_QSTR_stops) {
        lvgl_super_attr_check(attr, true, true, false, dest);
        if (dest[0] != MP_OBJ_SENTINEL) {
            mp_obj_t stops[LV_GRADIENT_MAX_STOPS] = { 0 };
            for (size_t i = 0; i < handle->dsc.stops_count; i++) {
                mp_obj_t items[] = {
                    mp_obj_new_int(lv_color_to_int(handle->dsc.stops[i].color)),
                    MP_OBJ_NEW_SMALL_INT(handle->dsc.stops[i].opa),
                    MP_OBJ_NEW_SMALL_INT(handle->dsc.stops[i].frac),
                };
                stops[i] = mp_obj_new_attrtuple(lvgl_gradient_stop_attrs, 3, items);
            }
            dest[0] = mp_obj_new_list(handle->dsc.stops_count, stops);
        }
        else if (dest[1] != MP_OBJ_NULL) {
            size_t len;
            mp_obj_t *items;
            if (mp_obj_is_type(dest[1], MP_OBJ_FROM_PTR(&mp_type_list))) {
                mp_obj_list_get(dest[1], &len, &items);
            }
            else if (mp_obj_is_type(dest[1], MP_OBJ_FROM_PTR(&mp_type_tuple))) {
                mp_obj_tuple_get(dest[1], &len, &items);
            }
            else {
                mp_raise_TypeError(NULL);
            }
            if (len > LV_GRADIENT_MAX_STOPS) {
                mp_raise_ValueError(MP_ERROR_TEXT("too many stops"));
            }
            lv_gradient_stop_t stops[LV_GRADIENT_MAX_STOPS] = { 0 };
            for (size_t i = 0; i < len; i++) {
                if (!mp_obj_is_type(items[i], MP_OBJ_FROM_PTR(&mp_type_tuple))) {
                    mp_raise_TypeError(NULL);
                }
                size_t tuple_len;
                mp_obj_t *tuple_items;                
                mp_obj_tuple_get(items[i], &tuple_len, &tuple_items);
                if (tuple_len != 3) {
                    mp_raise_TypeError(NULL);
                }
                stops[i].color = lv_color_hex(mp_obj_get_int(tuple_items[0]));
                stops[i].opa = mp_obj_get_int(tuple_items[1]);
                stops[i].frac = mp_obj_get_int(tuple_items[2]);
            }
            memcpy(handle->dsc.stops, stops, sizeof(stops));
            handle->dsc.stops_count = len;
            dest[0] = MP_OBJ_NULL;
        }
    }
    else {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

static const mp_rom_map_elem_t lvgl_grad_dsc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_ptr_del_obj) },
};
static MP_DEFINE_CONST_DICT(lvgl_grad_dsc_locals_dict, lvgl_grad_dsc_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_grad_dsc,
    MP_ROM_QSTR_CONST(MP_QSTR_GradDsc),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_grad_dsc_make_new,
    attr, lvgl_grad_dsc_attr,
    locals_dict, &lvgl_grad_dsc_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_grad_dsc);

static lvgl_ptr_t lvgl_grad_dsc_get_handle(const void *value) {
    // const lv_grad_dsc_t *dsc = value;
    return (void *)value - offsetof(lvgl_grad_dsc_handle_t, dsc);
}

const lvgl_ptr_type_t lvgl_grad_dsc_type = {
    &lvgl_type_grad_dsc,
    NULL,
    NULL,
    lvgl_grad_dsc_get_handle,
    NULL,
};


typedef struct lvgl_style_transition_dsc_handle {
    lvgl_ptr_handle_t base;
    lv_style_transition_dsc_t dsc;
} lvgl_style_transition_dsc_handle_t;

static mp_obj_t lvgl_style_transition_dsc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 4, 4, false);
    
    lvgl_style_transition_dsc_handle_t *handle = lv_malloc_zeroed(sizeof(lvgl_style_transition_dsc_handle_t));
    lvgl_ptr_init_handle(&handle->base, &lvgl_style_transition_dsc_type, &handle->dsc);
    mp_obj_t self_out = lvgl_ptr_to_mp(&handle->base);

    lv_style_prop_t *props = NULL;
    lvgl_type_from_mp(LV_TYPE_PROP_LIST, args[0], &props);

    lv_anim_path_cb_t path_cb = NULL;
    lvgl_type_from_mp(LV_TYPE_ANIM_PATH, args[1], &path_cb);

    uint32_t time = mp_obj_get_int(args[2]);
    uint32_t delay = mp_obj_get_int(args[3]);
    
    lv_style_transition_dsc_init(&handle->dsc, props, path_cb, time, delay, handle);
    return self_out;
}

static const mp_rom_map_elem_t lvgl_style_transition_dsc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&lvgl_ptr_del_obj) },
};
static MP_DEFINE_CONST_DICT(lvgl_style_transition_dsc_locals_dict, lvgl_style_transition_dsc_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_style_transition_dsc,
    MP_ROM_QSTR_CONST(MP_QSTR_StyleTransitionDsc),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_style_transition_dsc_make_new,
    attr, lvgl_ptr_attr,
    locals_dict, &lvgl_style_transition_dsc_locals_dict
    );
MP_REGISTER_OBJECT(lvgl_type_style_transition_dsc);

static lvgl_ptr_t lvgl_style_transition_dsc_get_handle(const void *value) {
    const lv_style_transition_dsc_t *dsc = value;
    return dsc->user_data;
}

static const lvgl_type_attr_t lvgl_style_transition_dsc_attrs[] = {
    { MP_ROM_QSTR_CONST(MP_QSTR_props), offsetof(lv_style_transition_dsc_t, props), LV_TYPE_PROP_LIST },
    { MP_ROM_QSTR_CONST(MP_QSTR_path_cb), offsetof(lv_style_transition_dsc_t, path_xcb), LV_TYPE_ANIM_PATH },
    { MP_ROM_QSTR_CONST(MP_QSTR_time), offsetof(lv_style_transition_dsc_t, time), LV_TYPE_INT32 },
    { MP_ROM_QSTR_CONST(MP_QSTR_delay), offsetof(lv_style_transition_dsc_t, delay), LV_TYPE_INT32 },
    { 0 },
};
MP_REGISTER_STRUCT(lvgl_style_transition_dsc_attrs, lvgl_type_attr_t);

const lvgl_ptr_type_t lvgl_style_transition_dsc_type = {
    &lvgl_type_style_transition_dsc,
    NULL,
    NULL,
    lvgl_style_transition_dsc_get_handle,
    lvgl_style_transition_dsc_attrs,
};
