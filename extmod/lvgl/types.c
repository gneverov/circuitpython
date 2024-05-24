// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./anim.h"
#include "./color.h"
#include "./draw/buffer.h"
#include "./font.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./style.h"
#include "./super.h"
#include "./types.h"


static const lvgl_ptr_type_t *lvgl_type_is_ptr(lv_type_code_t type_code) {
    switch (type_code) {
        case LV_TYPE_ANIM: {
            return &lvgl_anim_type;
        }
        case LV_TYPE_DRAW_BUFFER: {
            return &lvgl_draw_buf_type;
        }
        case LV_TYPE_STYLE_TRANSITION_DSC: {
            return &lvgl_style_transition_dsc_type;
        }
        default: {
            return NULL;
        }
    }
}

static const lvgl_static_ptr_type_t *lvgl_type_is_static_ptr(lv_type_code_t type_code) {
    switch (type_code) {
        case LV_TYPE_ANIM_PATH:
            return &lvgl_anim_path_type;
        case LV_TYPE_COLOR_FILTER:
            return &lvgl_color_filter_type;
        case LV_TYPE_FONT:
            return &lvgl_font_type;
        default:
            return NULL;
    }
}

static void lvgl_type_free_ptr(const lvgl_ptr_type_t *ptr_type, void *value) {
        void **plv_ptr = value;
        lvgl_ptr_handle_t *handle = lvgl_ptr_from_lv(ptr_type, *plv_ptr);
        lvgl_ptr_delete(handle);
        *plv_ptr = NULL;
}

void lvgl_type_free(lv_type_code_t type_code, void *value) {
    switch (type_code) {
        case LV_TYPE_STR: {
            char **pvalue = value;
            lv_free(*pvalue);
            *pvalue = NULL;
            return;
        }
        case LV_TYPE_ANIM: {
            lvgl_type_free_ptr(&lvgl_anim_type, value);
            return;
        }
        case LV_TYPE_DRAW_BUFFER: {
            lvgl_type_free_ptr(&lvgl_draw_buf_type, value);
            return;
        }
        case LV_TYPE_STYLE_TRANSITION_DSC: {
            lvgl_type_free_ptr(&lvgl_style_transition_dsc_type, value);
            return;
        }        
        case LV_TYPE_OBJ_HANDLE: {
            lvgl_handle_t **pvalue = value;
            lvgl_ptr_delete(&(*pvalue)->base);
            *pvalue = NULL;
            return;
        }
        case LV_TYPE_PROP_LIST: {
            lv_style_prop_t **pprops = value;
            free(*pprops);
            *pprops = NULL;
            return;
        }
        default: {
            break;
        }
    }
    const lvgl_ptr_type_t *ptr_type = lvgl_type_is_ptr(type_code);
    if (ptr_type) {
        lvgl_type_free_ptr(ptr_type, value);
        return;
    }
}

static void lvgl_type_from_mp_ptr(const lvgl_ptr_type_t *ptr_type, mp_obj_t obj, void *value) {
    lvgl_ptr_handle_t *handle = lvgl_ptr_from_mp(ptr_type, obj);
    const void **plv_ptr = value;
    lvgl_type_free_ptr(ptr_type, plv_ptr);
    *plv_ptr = lvgl_ptr_to_lv(handle);
    lvgl_ptr_copy(handle);
}

void lvgl_type_from_mp_prop_list(mp_obj_t obj, lv_style_prop_t **pprops) {
    if (obj == mp_const_none) {
        free(*pprops);
        *pprops = NULL;
        return;
    }
        
    size_t props_len;
    mp_obj_t *props_items;
    if (mp_obj_is_type(obj, MP_OBJ_FROM_PTR(&mp_type_list))) {
        mp_obj_list_get(obj, &props_len, &props_items);
    }
    else if (mp_obj_is_type(obj, MP_OBJ_FROM_PTR(&mp_type_tuple))) {
        mp_obj_tuple_get(obj, &props_len, &props_items);
    }
    else {
        mp_raise_TypeError(NULL);
    }

    lv_style_prop_t *props = malloc(sizeof(lv_style_prop_t) * (props_len + 1));
    for (size_t i = 0; i < props_len; i++) {
        const char* prop_str = mp_obj_str_get_str(props_items[i]);
        qstr prop_qstr = qstr_find_strn(prop_str, strlen(prop_str));
        lv_type_code_t type_code;
        lv_style_prop_t prop = lvgl_style_lookup(prop_qstr, &type_code);
        if (!prop) {
            free(props);
            mp_raise_msg_varg(&mp_type_AttributeError, MP_ERROR_TEXT("no style attribute '%s'"), prop_str);
        }
        props[i] = prop;
    }
    props[props_len] = 0;

    free(*pprops);
    *pprops = props;
}

void lvgl_type_from_mp(lv_type_code_t type_code, mp_obj_t obj, void *value) {
    if (obj == mp_const_none) {
        mp_raise_ValueError(NULL);
    }
    switch (type_code) {
        case LV_TYPE_INT8: {
            *(int8_t *)value = mp_obj_get_int(obj);
            return;
        }
        case LV_TYPE_INT16: {
            *(int16_t *)value = mp_obj_get_int(obj);
            return;
        }                
        case LV_TYPE_INT32: {
            *(int32_t *)value = mp_obj_get_int(obj);
            return;
        }
        case LV_TYPE_COLOR: {
            *(lv_color_t *)value = lv_color_hex(mp_obj_get_int(obj));
            return;
        }        
        case LV_TYPE_STR: {
            const char *str = mp_obj_str_get_str(obj);
            char **pvalue = value;
            lv_free(*pvalue);
            *pvalue = lv_strdup(str);
            return;
        }
        case LV_TYPE_OBJ_HANDLE: {
            lvgl_handle_t *handle = lvgl_obj_get_checked(obj);
            lvgl_handle_t **pvalue = value;
            lvgl_ptr_delete(&(*pvalue)->base);
            *pvalue = lvgl_obj_copy(handle);
            return;
        }
        case LV_TYPE_PROP_LIST: {
            lvgl_type_from_mp_prop_list(obj, value);
            return;
        }
        default: {
            break;
        }
    }
    const lvgl_ptr_type_t *ptr_type = lvgl_type_is_ptr(type_code);
    if (ptr_type) {
        lvgl_type_from_mp_ptr(ptr_type, obj, value);
        return;
    }    
    const lvgl_static_ptr_type_t *static_ptr_type = lvgl_type_is_static_ptr(type_code);
    if (static_ptr_type) {
        *(const void **)value = lvgl_static_ptr_from_mp(static_ptr_type, obj);
        return;
    }
    assert(0);
}

static mp_obj_t lvgl_type_to_mp_ptr(const lvgl_ptr_type_t *ptr_type, const void *value) {
    const void *lv_ptr = *(const void **)value;
    lvgl_ptr_handle_t *handle = lvgl_ptr_from_lv(ptr_type, lv_ptr);
    return lvgl_ptr_to_mp(handle);
}

static mp_obj_t lvgl_type_to_mp_prop_list(const lv_style_prop_t *props) {
    if (!props) {
        return mp_const_none;
    }
    mp_obj_t obj = mp_obj_new_list(0, NULL);
    while (*props) {
        assert(0);
        props++;
    }
    return obj;
}

mp_obj_t lvgl_type_to_mp(lv_type_code_t type_code, const void *value) {
    switch (type_code) {
        case LV_TYPE_INT8: {
            return mp_obj_new_int(*(int8_t *)value);
        }
        case LV_TYPE_INT16: {
            return mp_obj_new_int(*(int16_t *)value);
        }        
        case LV_TYPE_INT32: {
            return mp_obj_new_int(*(int32_t *)value);
        }        
        case LV_TYPE_COLOR: {
            return mp_obj_new_int(lv_color_to_int(*(lv_color_t *)value));
        }        
        case LV_TYPE_STR: {
            const char *str = *(char **)value;
            return str ? mp_obj_new_str(str, lv_strlen(str)) : mp_const_none;
        }     
        case LV_TYPE_OBJ_HANDLE: {
            return lvgl_obj_to_mp(*(lvgl_handle_t **)value);
        }
        case LV_TYPE_PROP_LIST: {
            return lvgl_type_to_mp_prop_list(*(const lv_style_prop_t **)value);
        }
        default: {
            break;
        }
    }
    const lvgl_ptr_type_t *ptr_type = lvgl_type_is_ptr(type_code);
    if (ptr_type) {
        return lvgl_type_to_mp_ptr(ptr_type, value);
    }      
    const lvgl_static_ptr_type_t *static_ptr_type = lvgl_type_is_static_ptr(type_code);
    if (static_ptr_type) {
        return lvgl_static_ptr_to_mp(static_ptr_type, *(const void **)value);
    }
    assert(0);
    return MP_OBJ_NULL;
}

void lvgl_type_clone_ptr(const lvgl_ptr_type_t *ptr_type, void *dst, const void *src) {
    const void *lv_src = *(const void **)src;
    lvgl_ptr_handle_t *handle = lvgl_ptr_from_lv(ptr_type, lv_src);

    void **plv_dst = dst;
    lvgl_type_free_ptr(ptr_type, plv_dst);
    *plv_dst = lvgl_ptr_to_lv(handle);
    lvgl_ptr_copy(handle);    
}

static void lvgl_type_clone_prop_list(lv_style_prop_t **dst, const lv_style_prop_t *src) {
    if (src) {
        size_t len = 0;
        for(const lv_style_prop_t *props = src; *props; props++) {
            len++;
        }
        size_t size = sizeof(lv_style_prop_t) * (len + 1);
        lv_free(*dst);
        *dst = lv_malloc(size);
        lv_memcpy(*dst, src, size);
    }
    else {
        lv_free(*dst);
        *dst = NULL;
    }
}

void lvgl_type_clone(lv_type_code_t type_code, void *dst, const void *src) {
    switch (type_code) {
        case LV_TYPE_INT8: {
            *(int8_t *)dst = *(int8_t *)src;
            return;
        }
        case LV_TYPE_INT16: {
            *(int16_t *)dst = *(int16_t *)src;
            return;
        }        
        case LV_TYPE_INT32: {
            *(int32_t *)dst = *(int32_t *)src;
            return;
        }        
        case LV_TYPE_COLOR: {
            *(lv_color_t *)dst = *(lv_color_t *)src;
            return;
        }        
        case LV_TYPE_STR: {
            *(char **)dst = lv_strdup(*(const char **)src);
            return;
        }   
        case LV_TYPE_OBJ_HANDLE: {
            lvgl_handle_t *handle = *(lvgl_handle_t **)src;
            lvgl_ptr_copy(&handle->base);
            *(lvgl_handle_t **)dst = handle;
            return;
        }
        case LV_TYPE_PROP_LIST: {
            lvgl_type_clone_prop_list(dst, *(const lv_style_prop_t **)src);
            return;
        }
        default: {
            break;
        }
    }
    const lvgl_ptr_type_t *ptr_type = lvgl_type_is_ptr(type_code);
    if (ptr_type) {
        lvgl_type_clone_ptr(ptr_type, dst, src);
        return;
    }       
    const lvgl_static_ptr_type_t *static_ptr_type = lvgl_type_is_static_ptr(type_code);
    if (static_ptr_type) {
        *(const void **)dst = *(const void **)src;
        return;
    }
    assert(0);
}


void lvgl_attrs_free(const lvgl_type_attr_t *attrs, void *value) {
    for (; attrs->qstr; attrs++) {
        lvgl_type_free(attrs->type_code, value + attrs->offset);
    }
}

void lvgl_type_attr(qstr attr, mp_obj_t *dest, lv_type_code_t type_code, void *value) {
    lvgl_super_attr_check(attr, true, true, false, dest);

    if (dest[0] != MP_OBJ_SENTINEL) {
        dest[0] = lvgl_type_to_mp(type_code, value);
    }
    else if (dest[1] != MP_OBJ_NULL) {
        lvgl_type_from_mp(type_code, dest[1], (void *)value);
        dest[0] = MP_OBJ_NULL;
    }
}

bool lvgl_attrs_attr(qstr attr, mp_obj_t *dest, const lvgl_type_attr_t *attrs, void *value) {
    for (; attrs->qstr; attrs++) {
        if (attrs->qstr == attr) {
            lvgl_type_attr(attr, dest, attrs->type_code, value + attrs->offset);
            return true;
        }
    }
    return false;
}

uint lvgl_bitfield_attr_bool(qstr attr, mp_obj_t *dest, uint value) {
    lvgl_super_attr_check(attr, true, true, false, dest);
    if (dest[0] != MP_OBJ_SENTINEL) {
        dest[0] = mp_obj_new_bool(value);
    }
    else if (dest[1] != MP_OBJ_NULL) {
        value = mp_obj_is_true(dest[1]);
        dest[0] = MP_OBJ_NULL;
    }
    return value;
}

uint lvgl_bitfield_attr_int(qstr attr, mp_obj_t *dest, uint value) {
    lvgl_super_attr_check(attr, true, true, false, dest);
    if (dest[0] != MP_OBJ_SENTINEL) {
        dest[0] = mp_obj_new_int(value);
    }
    else if (dest[1] != MP_OBJ_NULL) {
        value = mp_obj_get_int(dest[1]);
        dest[0] = MP_OBJ_NULL;
    }
    return value;
}
