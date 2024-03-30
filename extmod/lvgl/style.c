// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdlib.h>

#include "extmod/freeze/extmod.h"
#include "./style.h"


static struct lvgl_style {
    qstr_short_t qstr;
    lv_style_prop_t prop;
    uint8_t type;
} lvgl_style_table[] = {
    /*Group 0*/
    { MP_ROM_QSTR_CONST(MP_QSTR_width), LV_STYLE_WIDTH, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_height), LV_STYLE_HEIGHT, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_min_width), LV_STYLE_MIN_WIDTH, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_max_width), LV_STYLE_MAX_WIDTH, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_min_height), LV_STYLE_MIN_HEIGHT, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_max_height), LV_STYLE_MAX_HEIGHT, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_x), LV_STYLE_X, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_y), LV_STYLE_Y, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_align), LV_STYLE_ALIGN, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_radius), LV_STYLE_RADIUS, 0 },

    /*Group 1*/
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_top), LV_STYLE_PAD_TOP, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_bottom), LV_STYLE_PAD_BOTTOM, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_left), LV_STYLE_PAD_LEFT, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_right), LV_STYLE_PAD_RIGHT, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_pad_row), LV_STYLE_PAD_ROW, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_pad_column), LV_STYLE_PAD_COLUMN, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_layout), LV_STYLE_LAYOUT, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_margin_top), LV_STYLE_MARGIN_TOP, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_margin_bottom), LV_STYLE_MARGIN_BOTTOM, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_margin_left), LV_STYLE_MARGIN_LEFT, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_margin_right), LV_STYLE_MARGIN_RIGHT, 0 },

    /*Group 2*/
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_color), LV_STYLE_BG_COLOR, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_opa), LV_STYLE_BG_OPA, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad_dir), LV_STYLE_BG_GRAD_DIR, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_main_stop), LV_STYLE_BG_MAIN_STOP, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad_stop), LV_STYLE_BG_GRAD_STOP, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad_color), LV_STYLE_BG_GRAD_COLOR, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_bg_main_opa), LV_STYLE_BG_MAIN_OPA, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad_opa), LV_STYLE_BG_GRAD_OPA, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_bg_grad), LV_STYLE_BG_GRAD, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_base_dir), LV_STYLE_BASE_DIR, 0 },

    // { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_src), LV_STYLE_BG_IMAGE_SRC, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_opa), LV_STYLE_BG_IMAGE_OPA, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_recolor), LV_STYLE_BG_IMAGE_RECOLOR, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_recolor_opa), LV_STYLE_BG_IMAGE_RECOLOR_OPA, 0 },

    // { MP_ROM_QSTR_CONST(MP_QSTR_bg_image_tiled), LV_STYLE_BG_IMAGE_TILED, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_clip_corner), LV_STYLE_CLIP_CORNER, 0 },

    /*Group 3*/
    { MP_ROM_QSTR_CONST(MP_QSTR_border_width), LV_STYLE_BORDER_WIDTH, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_border_color), LV_STYLE_BORDER_COLOR, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_border_opa), LV_STYLE_BORDER_OPA, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_border_side), LV_STYLE_BORDER_SIDE, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_border_post), LV_STYLE_BORDER_POST, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_outline_width), LV_STYLE_OUTLINE_WIDTH, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_color), LV_STYLE_OUTLINE_COLOR, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_opa), LV_STYLE_OUTLINE_OPA, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_outline_pad), LV_STYLE_OUTLINE_PAD, 0 },

    /*Group 4*/
    // { MP_ROM_QSTR_CONST(MP_QSTR_shadow_width), LV_STYLE_SHADOW_WIDTH, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_shadow_color), LV_STYLE_SHADOW_COLOR, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_shadow_opa), LV_STYLE_SHADOW_OPA, 0 },

    // { MP_ROM_QSTR_CONST(MP_QSTR_shadow_offset_x), LV_STYLE_SHADOW_OFFSET_X, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_shadow_offset_y), LV_STYLE_SHADOW_OFFSET_Y, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_shadow_spread), LV_STYLE_SHADOW_SPREAD, 0 },

    // { MP_ROM_QSTR_CONST(MP_QSTR_image_opa), LV_STYLE_IMAGE_OPA, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_image_recolor), LV_STYLE_IMAGE_RECOLOR, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_image_recolor_opa), LV_STYLE_IMAGE_RECOLOR_OPA, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_line_width), LV_STYLE_LINE_WIDTH, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_dash_width), LV_STYLE_LINE_DASH_WIDTH, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_dash_gap), LV_STYLE_LINE_DASH_GAP, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_rounded), LV_STYLE_LINE_ROUNDED, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_color), LV_STYLE_LINE_COLOR, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_line_opa), LV_STYLE_LINE_OPA, 0 },

    /*Group 5*/
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_width), LV_STYLE_ARC_WIDTH, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_rounded), LV_STYLE_ARC_ROUNDED, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_color), LV_STYLE_ARC_COLOR, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_opa), LV_STYLE_ARC_OPA, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_arc_image_src), LV_STYLE_ARC_IMAGE_SRC, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_text_color), LV_STYLE_TEXT_COLOR, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_opa), LV_STYLE_TEXT_OPA, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_font), LV_STYLE_TEXT_FONT, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_text_letter_space), LV_STYLE_TEXT_LETTER_SPACE, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_line_space), LV_STYLE_TEXT_LINE_SPACE, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_decor), LV_STYLE_TEXT_DECOR, 0 },
    { MP_ROM_QSTR_CONST(MP_QSTR_text_align), LV_STYLE_TEXT_ALIGN, 0 },

    { MP_ROM_QSTR_CONST(MP_QSTR_opa), LV_STYLE_OPA, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_opa_layered), LV_STYLE_OPA_LAYERED, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_color_filter_dsc), LV_STYLE_COLOR_FILTER_DSC, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_color_filter_opa), LV_STYLE_COLOR_FILTER_OPA, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_anim), LV_STYLE_ANIM, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_anim_time), LV_STYLE_ANIM_TIME, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transition), LV_STYLE_TRANSITION, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_blend_mode), LV_STYLE_BLEND_MODE, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_width), LV_STYLE_TRANSFORM_WIDTH, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_height), LV_STYLE_TRANSFORM_HEIGHT, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_translate_x), LV_STYLE_TRANSLATE_X, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_translate_y), LV_STYLE_TRANSLATE_Y, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_scale_x), LV_STYLE_TRANSFORM_SCALE_X, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_scale_y), LV_STYLE_TRANSFORM_SCALE_Y, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_rotation), LV_STYLE_TRANSFORM_ROTATION, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_pivot_x), LV_STYLE_TRANSFORM_PIVOT_X, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_pivot_y), LV_STYLE_TRANSFORM_PIVOT_Y, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_skew_x), LV_STYLE_TRANSFORM_SKEW_X, 0 },
    // { MP_ROM_QSTR_CONST(MP_QSTR_transform_skew_y), LV_STYLE_TRANSFORM_SKEW_Y, 0 },  
};

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
}

lv_style_prop_t lvgl_style_lookup(qstr qstr) {
    struct lvgl_style key = { qstr, 0, 0 };
    struct lvgl_style *value = bsearch(&key, lvgl_style_table, sizeof(lvgl_style_table) / sizeof(struct lvgl_style), sizeof(struct lvgl_style), lvgl_style_compare);
    if (value->qstr != key.qstr) {
        return 0;
    }
    return value->prop;
}
