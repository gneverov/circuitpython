// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "./anim.h"
#include "./color.h"
#include "./display.h"
#include "./draw/moddraw.h"
#include "./font.h"
#include "./indev.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./queue.h"
#include "./style.h"
#include "./widgets/arc.h"
#include "./widgets/canvas.h"
#include "./widgets/image.h"
#include "./widgets/line.h"
#include "./widgets/widgets.h"

#include "./ft6206.h"
#include "./ili9341.h"


static SemaphoreHandle_t lvgl_mutex;
static TaskHandle_t lvgl_task;
static int lvgl_exit;

void lvgl_lock(void) {
    assert(!lvgl_is_locked());
    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
}

void lvgl_unlock(void) {
    assert(lvgl_is_locked());
    if (lvgl_task) {
        xTaskNotifyGive(lvgl_task);
    }
    xSemaphoreGive(lvgl_mutex);
}

void lvgl_lock_init(void) {
    lvgl_lock();
    if (!lv_is_initialized()) {
        lvgl_unlock();
        mp_raise_ValueError(MP_ERROR_TEXT("lvgl not initialized"));
    }
}

bool lvgl_is_locked(void) {
    return xSemaphoreGetMutexHolder(lvgl_mutex) == xTaskGetCurrentTaskHandle();
}

mp_obj_t lvgl_check_result(lv_result_t res) {
    if (res != LV_RESULT_OK) {
        mp_raise_ValueError(NULL);
    }
    return mp_const_none;
}

static uint32_t lvgl_tick(void) {
    return xTaskGetTickCount() * (1000 / configTICK_RATE_HZ);
}

static void lvgl_delay(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void lvgl_loop(void *params) {
    TaskHandle_t caller = params;

    xSemaphoreTake(lvgl_mutex, portMAX_DELAY);
    lv_init();
    lv_tick_set_cb(lvgl_tick);
    lv_delay_set_cb(lvgl_delay);
    xTaskNotifyGive(caller);

    lvgl_queue_default = lvgl_queue_alloc(20);
    lvgl_ptr_copy(&lvgl_queue_default->base);
    while (!lvgl_exit) {
        uint32_t sleep_ms = lv_task_handler();
        TickType_t sleep_ticks = sleep_ms == LV_NO_TIMER_READY ? portMAX_DELAY : pdMS_TO_TICKS(sleep_ms);
        xSemaphoreGive(lvgl_mutex);
        ulTaskNotifyTake(pdTRUE, sleep_ticks);
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);        
    }

    lvgl_queue_close(lvgl_queue_default);
    lvgl_ptr_delete(&lvgl_queue_default->base);
    lvgl_queue_default = NULL;
    lv_deinit();
    lvgl_task = NULL;
    xSemaphoreGive(lvgl_mutex);
    vTaskDelete(NULL);    
}

__attribute__((constructor))
void lvgl_ctor(void) {
    static StaticSemaphore_t xMutexBuffer;
    lvgl_mutex = xSemaphoreCreateMutexStatic(&xMutexBuffer);
}

mp_obj_t lvgl_init(void) {
    bool result = false;
    lvgl_lock();
    if (!lv_is_initialized()) {
        lvgl_exit = 0;
        lvgl_style_init();
        if (!xTaskCreate(lvgl_loop, "lvgl", 4096 / sizeof(StackType_t), xTaskGetCurrentTaskHandle(), 2, &lvgl_task)) {
            lvgl_unlock();
            mp_raise_OSError(MP_ENOMEM);
        }

        while (!lv_is_initialized()) {
            lvgl_unlock();
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            lvgl_lock();
        }
        result = true;
    }
    lvgl_unlock();

    return mp_obj_new_bool(result);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_init_obj, lvgl_init);

mp_obj_t lvgl_deinit(void) {
    lvgl_lock();
    if (lv_is_initialized()) {
        lvgl_exit = 1;

        while (lv_is_initialized()) {
            lvgl_unlock();
            vTaskDelay(1);
            lvgl_lock();
        }
    }
    lvgl_unlock();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_deinit_obj, lvgl_deinit);

STATIC mp_obj_t lvgl_run_forever(void) {
    lvgl_lock();
    mp_obj_t obj = lvgl_unlock_ptr(&lvgl_queue_default->base);

    if (obj == mp_const_none) {
        return mp_const_none;
    }

    mp_obj_t args[2];
    mp_load_method(obj, MP_QSTR_run, args);
    int ret = 0;
    do {
        nlr_buf_t nlr;
        if (nlr_push(&nlr) == 0) {
            ret = MP_OBJ_SMALL_INT_VALUE(mp_call_method_n_kw(0, 0, args));
            nlr_pop();
        }
        else {
            mp_obj_t exc = MP_OBJ_FROM_PTR(nlr.ret_val);
            if (mp_obj_exception_match(exc, &mp_type_KeyboardInterrupt) || mp_obj_exception_match(exc, &mp_type_SystemExit)) {
                nlr_jump(nlr.ret_val);
            }
            else {
                mp_obj_print_exception(&mp_plat_print, exc);
            }
        }
    }
    while (ret > 0);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_run_forever_obj, lvgl_run_forever);

STATIC mp_obj_t lvgl_getattr(mp_obj_t attr) {
    if (MP_OBJ_QSTR_VALUE(attr) == MP_QSTR_display) {
        return lvgl_display_get_default();
    }
    if (MP_OBJ_QSTR_VALUE(attr) == MP_QSTR_screen) {
        lvgl_lock();
        lv_obj_t *obj = lv_screen_active();
        lvgl_obj_handle_t *handle = lvgl_obj_from_lv(obj);
        return lvgl_unlock_ptr(&handle->base);
    }
    if (MP_OBJ_QSTR_VALUE(attr) == MP_QSTR_indevs) {
        return lvgl_indev_list();
    }    
    return MP_OBJ_NULL;
}
MP_DEFINE_CONST_FUN_OBJ_1(lvgl_getattr_obj, lvgl_getattr);

STATIC mp_obj_t lvgl_load_screen(mp_obj_t obj_in) {
    lvgl_obj_handle_t *handle = lvgl_obj_from_mp_checked(obj_in);
    lvgl_lock();
    lv_obj_t *scr = lvgl_lock_obj(handle);
    lv_screen_load(scr);
    lvgl_unlock();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_load_screen_obj, lvgl_load_screen);

STATIC mp_obj_t lvgl_color_make(mp_obj_t red_in, mp_obj_t green_in, mp_obj_t blue_in) {
    mp_int_t r = mp_obj_get_int(red_in);
    mp_int_t g = mp_obj_get_int(green_in);
    mp_int_t b = mp_obj_get_int(blue_in);
    lv_color_t c = lv_color_make(r, g, b);
    return mp_obj_new_int(lv_color_to_int(c));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(lvgl_color_make_obj, lvgl_color_make);

STATIC mp_obj_t lvgl_color_lighten(mp_obj_t c_in, mp_obj_t lvl_in) {
    lv_color_t c = lv_color_hex(mp_obj_get_int(c_in));
    lv_opa_t lvl = mp_obj_get_int(lvl_in);
    c = lv_color_lighten(c, lvl);
    return mp_obj_new_int(lv_color_to_int(c));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lvgl_color_lighten_obj, lvgl_color_lighten);

STATIC mp_obj_t lvgl_color_darken(mp_obj_t c_in, mp_obj_t lvl_in) {
    lv_color_t c = lv_color_hex(mp_obj_get_int(c_in));
    lv_opa_t lvl = mp_obj_get_int(lvl_in);
    c = lv_color_darken(c, lvl);
    return mp_obj_new_int(lv_color_to_int(c));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(lvgl_color_darken_obj, lvgl_color_darken);

STATIC mp_obj_t lvgl_color_black(void) {
    lv_color_t c = lv_color_black();
    return mp_obj_new_int(lv_color_to_int(c));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_color_black_obj, lvgl_color_black);

STATIC mp_obj_t lvgl_color_white(void) {
    lv_color_t c = lv_color_white();
    return mp_obj_new_int(lv_color_to_int(c));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_color_white_obj, lvgl_color_white);

STATIC mp_obj_t lvgl_pct(mp_obj_t value_in) {
    int32_t value = mp_obj_get_int(value_in);
    return mp_obj_new_int(lv_pct(value));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(lvgl_pct_obj, lvgl_pct);

STATIC const mp_rom_map_elem_t lvgl_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_lvgl) },
    { MP_ROM_QSTR(MP_QSTR_draw),            MP_ROM_PTR(&lvgl_module_draw) },
    { MP_ROM_QSTR(MP_QSTR___getattr__),     MP_ROM_PTR(&lvgl_getattr_obj) },
    { MP_ROM_QSTR(MP_QSTR_init),            MP_ROM_PTR(&lvgl_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit),          MP_ROM_PTR(&lvgl_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_run_forever),     MP_ROM_PTR(&lvgl_run_forever_obj) },
    { MP_ROM_QSTR(MP_QSTR_Arc),             MP_ROM_PTR(&lvgl_type_arc) },
    { MP_ROM_QSTR(MP_QSTR_Anim),            MP_ROM_PTR(&lvgl_type_anim) },
    { MP_ROM_QSTR(MP_QSTR_AnimPath),        MP_ROM_PTR(&lvgl_type_anim_path) },
    // { MP_ROM_QSTR(MP_QSTR_Class),           MP_ROM_PTR(&lvgl_type_class) },
    { MP_ROM_QSTR(MP_QSTR_Button),          MP_ROM_PTR(&lvgl_type_button) },
    { MP_ROM_QSTR(MP_QSTR_Canvas),          MP_ROM_PTR(&lvgl_type_canvas) },
    { MP_ROM_QSTR(MP_QSTR_ColorFilter),     MP_ROM_PTR(&lvgl_type_color_filter) },
    { MP_ROM_QSTR(MP_QSTR_Display),         MP_ROM_PTR(&lvgl_type_display) },
    { MP_ROM_QSTR(MP_QSTR_Font),            MP_ROM_PTR(&lvgl_type_font) },
    { MP_ROM_QSTR(MP_QSTR_GradDsc),         MP_ROM_PTR(&lvgl_type_grad_dsc) },
    { MP_ROM_QSTR(MP_QSTR_Image),           MP_ROM_PTR(&lvgl_type_image) },
    { MP_ROM_QSTR(MP_QSTR_InDev),           MP_ROM_PTR(&lvgl_type_indev) },
    { MP_ROM_QSTR(MP_QSTR_Label),           MP_ROM_PTR(&lvgl_type_label) },
    { MP_ROM_QSTR(MP_QSTR_Line),            MP_ROM_PTR(&lvgl_type_line) },
    { MP_ROM_QSTR(MP_QSTR_Object),          MP_ROM_PTR(&lvgl_type_obj) },
    { MP_ROM_QSTR(MP_QSTR_ObjectCollection), MP_ROM_PTR(&lvgl_type_obj_list) },
    { MP_ROM_QSTR(MP_QSTR_Palette),         MP_ROM_PTR(&lvgl_type_palette) },
    { MP_ROM_QSTR(MP_QSTR_Slider),          MP_ROM_PTR(&lvgl_type_slider) },
    { MP_ROM_QSTR(MP_QSTR_Style),           MP_ROM_PTR(&lvgl_type_style) },
    { MP_ROM_QSTR(MP_QSTR_StyleTransitionDsc), MP_ROM_PTR(&lvgl_type_style_transition_dsc) },
    { MP_ROM_QSTR(MP_QSTR_Switch),          MP_ROM_PTR(&lvgl_type_switch) },

    { MP_ROM_QSTR(MP_QSTR_FT6206),          MP_ROM_PTR(&lvgl_type_FT6206) },
    { MP_ROM_QSTR(MP_QSTR_ILI9341),         MP_ROM_PTR(&lvgl_type_ILI9341) },

    { MP_ROM_QSTR(MP_QSTR_load_screen),     MP_ROM_PTR(&lvgl_load_screen_obj) },

    { MP_ROM_QSTR(MP_QSTR_color_make),      MP_ROM_PTR(&lvgl_color_make_obj) },
    { MP_ROM_QSTR(MP_QSTR_color_lighten),   MP_ROM_PTR(&lvgl_color_lighten_obj) },
    { MP_ROM_QSTR(MP_QSTR_color_darken),    MP_ROM_PTR(&lvgl_color_darken_obj) },
    { MP_ROM_QSTR(MP_QSTR_color_black),     MP_ROM_PTR(&lvgl_color_black_obj) },
    { MP_ROM_QSTR(MP_QSTR_color_white),     MP_ROM_PTR(&lvgl_color_white_obj) },

    { MP_ROM_QSTR(MP_QSTR_pct),             MP_ROM_PTR(&lvgl_pct_obj) },


    // enum lv_align_t
    { MP_ROM_QSTR(MP_QSTR_ALIGN_DEFAULT),       MP_ROM_INT(LV_ALIGN_DEFAULT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_TOP_LEFT),      MP_ROM_INT(LV_ALIGN_TOP_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_TOP_MID),       MP_ROM_INT(LV_ALIGN_TOP_MID) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_TOP_RIGHT),     MP_ROM_INT(LV_ALIGN_TOP_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_BOTTOM_LEFT),   MP_ROM_INT(LV_ALIGN_BOTTOM_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_BOTTOM_MID),    MP_ROM_INT(LV_ALIGN_BOTTOM_MID) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_BOTTOM_RIGHT),  MP_ROM_INT(LV_ALIGN_BOTTOM_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_LEFT_MID),      MP_ROM_INT(LV_ALIGN_LEFT_MID) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_RIGHT_MID),     MP_ROM_INT(LV_ALIGN_RIGHT_MID) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_CENTER),        MP_ROM_INT(LV_ALIGN_CENTER) },

    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_TOP_LEFT),      MP_ROM_INT(LV_ALIGN_OUT_TOP_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_TOP_MID),       MP_ROM_INT(LV_ALIGN_OUT_TOP_MID) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_TOP_RIGHT),     MP_ROM_INT(LV_ALIGN_OUT_TOP_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_BOTTOM_LEFT),   MP_ROM_INT(LV_ALIGN_OUT_BOTTOM_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_BOTTOM_MID),    MP_ROM_INT(LV_ALIGN_OUT_BOTTOM_MID) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_BOTTOM_RIGHT),  MP_ROM_INT(LV_ALIGN_OUT_BOTTOM_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_LEFT_TOP),      MP_ROM_INT(LV_ALIGN_OUT_LEFT_TOP) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_LEFT_MID),      MP_ROM_INT(LV_ALIGN_OUT_LEFT_MID) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_LEFT_BOTTOM),   MP_ROM_INT(LV_ALIGN_OUT_LEFT_BOTTOM) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_RIGHT_TOP),     MP_ROM_INT(LV_ALIGN_OUT_RIGHT_TOP) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_RIGHT_MID),     MP_ROM_INT(LV_ALIGN_OUT_RIGHT_MID) },
    { MP_ROM_QSTR(MP_QSTR_ALIGN_OUT_RIGHT_BOTTOM),  MP_ROM_INT(LV_ALIGN_OUT_RIGHT_BOTTOM) },


    // enum lv_base_dir_t
    { MP_ROM_QSTR(MP_QSTR_BASE_DIR_LTR),        MP_ROM_INT(LV_BASE_DIR_LTR) },
    { MP_ROM_QSTR(MP_QSTR_BASE_DIR_RTL),        MP_ROM_INT(LV_BASE_DIR_RTL) },
    { MP_ROM_QSTR(MP_QSTR_BASE_DIR_AUTO),       MP_ROM_INT(LV_BASE_DIR_AUTO) },

    { MP_ROM_QSTR(MP_QSTR_BASE_DIR_NEUTRAL),    MP_ROM_INT(LV_BASE_DIR_NEUTRAL) },
    { MP_ROM_QSTR(MP_QSTR_BASE_DIR_WEAK),       MP_ROM_INT(LV_BASE_DIR_WEAK) },


    // enum lv_label_long_mode_t
    { MP_ROM_QSTR(MP_QSTR_LABEL_LONG_WRAP),             MP_ROM_INT(LV_LABEL_LONG_WRAP) },
    { MP_ROM_QSTR(MP_QSTR_LABEL_LONG_DOT),              MP_ROM_INT(LV_LABEL_LONG_DOT) },
    { MP_ROM_QSTR(MP_QSTR_LABEL_LONG_SCROLL),           MP_ROM_INT(LV_LABEL_LONG_SCROLL) },
    { MP_ROM_QSTR(MP_QSTR_LABEL_LONG_SCROLL_CIRCULAR),  MP_ROM_INT(LV_LABEL_LONG_SCROLL_CIRCULAR) },
    { MP_ROM_QSTR(MP_QSTR_LABEL_LONG_CLIP),             MP_ROM_INT(LV_LABEL_LONG_CLIP) },
    

    { MP_ROM_QSTR(MP_QSTR_ANIM_REPEAT_INFINITE),   MP_ROM_INT(LV_ANIM_REPEAT_INFINITE) },

    { MP_ROM_QSTR(MP_QSTR_RADIUS_CIRCLE),   MP_ROM_INT(LV_RADIUS_CIRCLE) },
    { MP_ROM_QSTR(MP_QSTR_SIZE_CONTENT),    MP_ROM_INT(LV_SIZE_CONTENT) },


    // enum lv_state_t
    { MP_ROM_QSTR(MP_QSTR_STATE_DEFAULT),   MP_ROM_INT(LV_STATE_DEFAULT) },
    { MP_ROM_QSTR(MP_QSTR_STATE_CHECKED),   MP_ROM_INT(LV_STATE_CHECKED) },
    { MP_ROM_QSTR(MP_QSTR_STATE_FOCUSED),   MP_ROM_INT(LV_STATE_FOCUSED) },
    { MP_ROM_QSTR(MP_QSTR_STATE_FOCUS_KEY), MP_ROM_INT(LV_STATE_FOCUS_KEY) },
    { MP_ROM_QSTR(MP_QSTR_STATE_EDITED),    MP_ROM_INT(LV_STATE_EDITED) },
    { MP_ROM_QSTR(MP_QSTR_STATE_HOVERED),   MP_ROM_INT(LV_STATE_HOVERED) },
    { MP_ROM_QSTR(MP_QSTR_STATE_PRESSED),   MP_ROM_INT(LV_STATE_PRESSED) },
    { MP_ROM_QSTR(MP_QSTR_STATE_SCROLLED),  MP_ROM_INT(LV_STATE_SCROLLED) },
    { MP_ROM_QSTR(MP_QSTR_STATE_DISABLED),  MP_ROM_INT(LV_STATE_DISABLED) },
    { MP_ROM_QSTR(MP_QSTR_STATE_ANY),       MP_ROM_INT(LV_STATE_ANY) },


    // enum lv_part_t
    { MP_ROM_QSTR(MP_QSTR_PART_MAIN),       MP_ROM_INT(LV_PART_MAIN) },
    { MP_ROM_QSTR(MP_QSTR_PART_SCROLLBAR),  MP_ROM_INT(LV_PART_SCROLLBAR) },
    { MP_ROM_QSTR(MP_QSTR_PART_INDICATOR),  MP_ROM_INT(LV_PART_INDICATOR) },
    { MP_ROM_QSTR(MP_QSTR_PART_KNOB),       MP_ROM_INT(LV_PART_KNOB) },
    { MP_ROM_QSTR(MP_QSTR_PART_SELECTED),   MP_ROM_INT(LV_PART_SELECTED) },
    { MP_ROM_QSTR(MP_QSTR_PART_ITEMS),      MP_ROM_INT(LV_PART_ITEMS) },
    { MP_ROM_QSTR(MP_QSTR_PART_CURSOR),     MP_ROM_INT(LV_PART_CURSOR) },
    { MP_ROM_QSTR(MP_QSTR_PART_ANY),        MP_ROM_INT(LV_PART_ANY) },


    // enum lv_obj_flag_t
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_HIDDEN),                 MP_ROM_INT(LV_OBJ_FLAG_HIDDEN) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_CLICKABLE),              MP_ROM_INT(LV_OBJ_FLAG_CLICKABLE) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_CLICK_FOCUSABLE),        MP_ROM_INT(LV_OBJ_FLAG_CLICK_FOCUSABLE) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_CHECKABLE),              MP_ROM_INT(LV_OBJ_FLAG_CHECKABLE) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLLABLE),             MP_ROM_INT(LV_OBJ_FLAG_SCROLLABLE) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLL_ELASTIC),         MP_ROM_INT(LV_OBJ_FLAG_SCROLL_ELASTIC) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLL_MOMENTUM),        MP_ROM_INT(LV_OBJ_FLAG_SCROLL_MOMENTUM) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLL_ONE),             MP_ROM_INT(LV_OBJ_FLAG_SCROLL_ONE) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLL_CHAIN_HOR),       MP_ROM_INT(LV_OBJ_FLAG_SCROLL_CHAIN_HOR) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLL_CHAIN_VER),       MP_ROM_INT(LV_OBJ_FLAG_SCROLL_CHAIN_VER) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLL_CHAIN),           MP_ROM_INT(LV_OBJ_FLAG_SCROLL_CHAIN) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLL_ON_FOCUS),        MP_ROM_INT(LV_OBJ_FLAG_SCROLL_ON_FOCUS) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SCROLL_WITH_ARROW),      MP_ROM_INT(LV_OBJ_FLAG_SCROLL_WITH_ARROW) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SNAPPABLE),              MP_ROM_INT(LV_OBJ_FLAG_SNAPPABLE) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_PRESS_LOCK),             MP_ROM_INT(LV_OBJ_FLAG_PRESS_LOCK) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_EVENT_BUBBLE),           MP_ROM_INT(LV_OBJ_FLAG_EVENT_BUBBLE) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_GESTURE_BUBBLE),         MP_ROM_INT(LV_OBJ_FLAG_GESTURE_BUBBLE) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_ADV_HITTEST),            MP_ROM_INT(LV_OBJ_FLAG_ADV_HITTEST) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_IGNORE_LAYOUT),          MP_ROM_INT(LV_OBJ_FLAG_IGNORE_LAYOUT) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_FLOATING),               MP_ROM_INT(LV_OBJ_FLAG_FLOATING) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_SEND_DRAW_TASK_EVENTS),  MP_ROM_INT(LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS) },
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_OVERFLOW_VISIBLE),       MP_ROM_INT(LV_OBJ_FLAG_OVERFLOW_VISIBLE) },
#if LV_USE_FLEX
    { MP_ROM_QSTR(MP_QSTR_OBJ_FLAG_FLEX_IN_NEW_TRACK),      MP_ROM_INT(LV_OBJ_FLAG_FLEX_IN_NEW_TRACK) },
#endif


    // enum lv_text_align_t
    { MP_ROM_QSTR(MP_QSTR_TEXT_ALIGN_AUTO),     MP_ROM_INT(LV_TEXT_ALIGN_AUTO), },
    { MP_ROM_QSTR(MP_QSTR_TEXT_ALIGN_LEFT),     MP_ROM_INT(LV_TEXT_ALIGN_LEFT), },
    { MP_ROM_QSTR(MP_QSTR_TEXT_ALIGN_CENTER),   MP_ROM_INT(LV_TEXT_ALIGN_CENTER), },
    { MP_ROM_QSTR(MP_QSTR_TEXT_ALIGN_RIGHT),    MP_ROM_INT(LV_TEXT_ALIGN_RIGHT), },


    // enum lv_text_decor_t
    { MP_ROM_QSTR(MP_QSTR_TEXT_DECOR_NONE),             MP_ROM_INT(LV_TEXT_DECOR_NONE) },
    { MP_ROM_QSTR(MP_QSTR_TEXT_DECOR_UNDERLINE),        MP_ROM_INT(LV_TEXT_DECOR_UNDERLINE) },
    { MP_ROM_QSTR(MP_QSTR_TEXT_DECOR_STRIKETHROUGH),    MP_ROM_INT(LV_TEXT_DECOR_STRIKETHROUGH) },


    // enum lv_layout_t
    { MP_ROM_QSTR(MP_QSTR_LAYOUT_NONE), MP_ROM_INT(LV_LAYOUT_NONE) },
#if LV_USE_FLEX
    { MP_ROM_QSTR(MP_QSTR_LAYOUT_FLEX), MP_ROM_INT(LV_LAYOUT_FLEX) },
#endif
#if LV_USE_GRID
    { MP_ROM_QSTR(MP_QSTR_LAYOUT_GRID), MP_ROM_INT(LV_LAYOUT_GRID) },
#endif


    // lv_flex_align_t enum
    { MP_ROM_QSTR(MP_QSTR_FLEX_ALIGN_START),            MP_ROM_INT(LV_FLEX_ALIGN_START) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_ALIGN_END),              MP_ROM_INT(LV_FLEX_ALIGN_END) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_ALIGN_CENTER),           MP_ROM_INT(LV_FLEX_ALIGN_CENTER) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_ALIGN_SPACE_EVENLY),     MP_ROM_INT(LV_FLEX_ALIGN_SPACE_EVENLY) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_ALIGN_SPACE_AROUND),     MP_ROM_INT(LV_FLEX_ALIGN_SPACE_AROUND) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_ALIGN_SPACE_BETWEEN),    MP_ROM_INT(LV_FLEX_ALIGN_SPACE_BETWEEN) },


    // enum lv_flex_flow_t
    { MP_ROM_QSTR(MP_QSTR_FLEX_FLOW_ROW),                   MP_ROM_INT(LV_FLEX_FLOW_ROW) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_FLOW_COLUMN),                MP_ROM_INT(LV_FLEX_FLOW_COLUMN) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_FLOW_ROW_WRAP),              MP_ROM_INT(LV_FLEX_FLOW_ROW_WRAP) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_FLOW_ROW_REVERSE),           MP_ROM_INT(LV_FLEX_FLOW_ROW_REVERSE) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_FLOW_ROW_WRAP_REVERSE),      MP_ROM_INT(LV_FLEX_FLOW_ROW_WRAP_REVERSE) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_FLOW_COLUMN_WRAP),           MP_ROM_INT(LV_FLEX_FLOW_COLUMN_WRAP) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_FLOW_COLUMN_REVERSE),        MP_ROM_INT(LV_FLEX_FLOW_COLUMN_REVERSE) },
    { MP_ROM_QSTR(MP_QSTR_FLEX_FLOW_COLUMN_WRAP_REVERSE),   MP_ROM_INT(LV_FLEX_FLOW_COLUMN_WRAP_REVERSE) },


    // enum lv_event_code_t
     { MP_ROM_QSTR(MP_QSTR_EVENT_ALL),  MP_ROM_INT(LV_EVENT_ALL) },

    /** Input device events*/
    { MP_ROM_QSTR(MP_QSTR_EVENT_PRESSED),               MP_ROM_INT(LV_EVENT_PRESSED) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_PRESSING),              MP_ROM_INT(LV_EVENT_PRESSING) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_PRESS_LOST),            MP_ROM_INT(LV_EVENT_PRESS_LOST) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_SHORT_CLICKED),         MP_ROM_INT(LV_EVENT_SHORT_CLICKED) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_LONG_PRESSED),          MP_ROM_INT(LV_EVENT_LONG_PRESSED) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_LONG_PRESSED_REPEAT),   MP_ROM_INT(LV_EVENT_LONG_PRESSED_REPEAT) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_CLICKED),               MP_ROM_INT(LV_EVENT_CLICKED) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_RELEASED),              MP_ROM_INT(LV_EVENT_RELEASED) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_SCROLL_BEGIN),          MP_ROM_INT(LV_EVENT_SCROLL_BEGIN) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_SCROLL_THROW_BEGIN),    MP_ROM_INT(LV_EVENT_SCROLL_THROW_BEGIN) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_SCROLL_END),            MP_ROM_INT(LV_EVENT_SCROLL_END) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_SCROLL),                MP_ROM_INT(LV_EVENT_SCROLL) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_GESTURE),               MP_ROM_INT(LV_EVENT_GESTURE) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_KEY),                   MP_ROM_INT(LV_EVENT_KEY) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_FOCUSED),               MP_ROM_INT(LV_EVENT_FOCUSED) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_DEFOCUSED),             MP_ROM_INT(LV_EVENT_DEFOCUSED) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_LEAVE),                 MP_ROM_INT(LV_EVENT_LEAVE) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_HIT_TEST),              MP_ROM_INT(LV_EVENT_HIT_TEST) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_INDEV_RESET),           MP_ROM_INT(LV_EVENT_INDEV_RESET) },

    /** Drawing events*/
    // { MP_ROM_QSTR(MP_QSTR_EVENT_COVER_CHECK),           MP_ROM_INT(LV_EVENT_COVER_CHECK) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_REFR_EXT_DRAW_SIZE),    MP_ROM_INT(LV_EVENT_REFR_EXT_DRAW_SIZE) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_DRAW_MAIN_BEGIN),       MP_ROM_INT(LV_EVENT_DRAW_MAIN_BEGIN) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_DRAW_MAIN),             MP_ROM_INT(LV_EVENT_DRAW_MAIN) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_DRAW_MAIN_END),         MP_ROM_INT(LV_EVENT_DRAW_MAIN_END) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_DRAW_POST_BEGIN),       MP_ROM_INT(LV_EVENT_DRAW_POST_BEGIN) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_DRAW_POST),             MP_ROM_INT(LV_EVENT_DRAW_POST) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_DRAW_POST_END),         MP_ROM_INT(LV_EVENT_DRAW_POST_END) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_DRAW_TASK_ADDED),       MP_ROM_INT(LV_EVENT_DRAW_TASK_ADDED) },

    /** Special events*/
    { MP_ROM_QSTR(MP_QSTR_EVENT_VALUE_CHANGED), MP_ROM_INT(LV_EVENT_VALUE_CHANGED) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_INSERT),        MP_ROM_INT(LV_EVENT_INSERT) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_REFRESH),       MP_ROM_INT(LV_EVENT_REFRESH) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_READY),         MP_ROM_INT(LV_EVENT_READY) },
    { MP_ROM_QSTR(MP_QSTR_EVENT_CANCEL),        MP_ROM_INT(LV_EVENT_CANCEL) },

    /** Other events*/
    // { MP_ROM_QSTR(MP_QSTR_EVENT_CREATE),                MP_ROM_INT(LV_EVENT_CREATE) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_DELETE),                MP_ROM_INT(LV_EVENT_DELETE) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_CHILD_CHANGED),         MP_ROM_INT(LV_EVENT_CHILD_CHANGED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_CHILD_CREATED),         MP_ROM_INT(LV_EVENT_CHILD_CREATED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_CHILD_DELETED),         MP_ROM_INT(LV_EVENT_CHILD_DELETED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_SCREEN_UNLOAD_START),   MP_ROM_INT(LV_EVENT_SCREEN_UNLOAD_START) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_SCREEN_LOAD_START),     MP_ROM_INT(LV_EVENT_SCREEN_LOAD_START) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_SCREEN_LOADED),         MP_ROM_INT(LV_EVENT_SCREEN_LOADED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_SCREEN_UNLOADED),       MP_ROM_INT(LV_EVENT_SCREEN_UNLOADED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_SIZE_CHANGED),          MP_ROM_INT(LV_EVENT_SIZE_CHANGED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_STYLE_CHANGED),         MP_ROM_INT(LV_EVENT_STYLE_CHANGED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_LAYOUT_CHANGED),        MP_ROM_INT(LV_EVENT_LAYOUT_CHANGED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_GET_SELF_SIZE),         MP_ROM_INT(LV_EVENT_GET_SELF_SIZE) },

    /** Events of optional LVGL components*/
    // { MP_ROM_QSTR(MP_QSTR_EVENT_INVALIDATE_AREA),       MP_ROM_INT(LV_EVENT_INVALIDATE_AREA) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_RESOLUTION_CHANGED),    MP_ROM_INT(LV_EVENT_RESOLUTION_CHANGED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_COLOR_FORMAT_CHANGED),  MP_ROM_INT(LV_EVENT_COLOR_FORMAT_CHANGED) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_REFR_REQUEST),          MP_ROM_INT(LV_EVENT_REFR_REQUEST) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_REFR_START),            MP_ROM_INT(LV_EVENT_REFR_START) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_REFR_READY),            MP_ROM_INT(LV_EVENT_REFR_READY) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_RENDER_START),          MP_ROM_INT(LV_EVENT_RENDER_START) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_RENDER_READY),          MP_ROM_INT(LV_EVENT_RENDER_READY) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_FLUSH_START),           MP_ROM_INT(LV_EVENT_FLUSH_START) },
    // { MP_ROM_QSTR(MP_QSTR_EVENT_FLUSH_FINISH),          MP_ROM_INT(LV_EVENT_FLUSH_FINISH) },

    // { MP_ROM_QSTR(MP_QSTR_EVENT_PREPROCESS = 0x80), MP_ROM_INT(LV_EVENT_PREPROCESS = 0x80) },

    // enum lv_opa_t
    { MP_ROM_QSTR(MP_QSTR_OPA_TRANSP),      MP_ROM_INT(LV_OPA_TRANSP) },
    { MP_ROM_QSTR(MP_QSTR_OPA_0),           MP_ROM_INT(LV_OPA_0) },
    { MP_ROM_QSTR(MP_QSTR_OPA_10),          MP_ROM_INT(LV_OPA_10) },
    { MP_ROM_QSTR(MP_QSTR_OPA_20),          MP_ROM_INT(LV_OPA_20) },
    { MP_ROM_QSTR(MP_QSTR_OPA_30),          MP_ROM_INT(LV_OPA_30) },
    { MP_ROM_QSTR(MP_QSTR_OPA_40),          MP_ROM_INT(LV_OPA_40) },
    { MP_ROM_QSTR(MP_QSTR_OPA_50),          MP_ROM_INT(LV_OPA_50) },
    { MP_ROM_QSTR(MP_QSTR_OPA_60),          MP_ROM_INT(LV_OPA_60) },
    { MP_ROM_QSTR(MP_QSTR_OPA_70),          MP_ROM_INT(LV_OPA_70) },
    { MP_ROM_QSTR(MP_QSTR_OPA_80),          MP_ROM_INT(LV_OPA_80) },
    { MP_ROM_QSTR(MP_QSTR_OPA_90),          MP_ROM_INT(LV_OPA_90) },
    { MP_ROM_QSTR(MP_QSTR_OPA_100),         MP_ROM_INT(LV_OPA_100) },
    { MP_ROM_QSTR(MP_QSTR_OPA_COVER),       MP_ROM_INT(LV_OPA_COVER) },


    // enum lv_border_side_t
    { MP_ROM_QSTR(MP_QSTR_BORDER_SIDE_NONE),        MP_ROM_INT(LV_BORDER_SIDE_NONE) },
    { MP_ROM_QSTR(MP_QSTR_BORDER_SIDE_BOTTOM),      MP_ROM_INT(LV_BORDER_SIDE_BOTTOM) },
    { MP_ROM_QSTR(MP_QSTR_BORDER_SIDE_TOP),         MP_ROM_INT(LV_BORDER_SIDE_TOP) },
    { MP_ROM_QSTR(MP_QSTR_BORDER_SIDE_LEFT),        MP_ROM_INT(LV_BORDER_SIDE_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_BORDER_SIDE_RIGHT),       MP_ROM_INT(LV_BORDER_SIDE_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_BORDER_SIDE_FULL),        MP_ROM_INT(LV_BORDER_SIDE_FULL) },
    { MP_ROM_QSTR(MP_QSTR_BORDER_SIDE_INTERNAL),    MP_ROM_INT(LV_BORDER_SIDE_INTERNAL) },


    // enum lv_grad_dir_t
    { MP_ROM_QSTR(MP_QSTR_GRAD_DIR_NONE),   MP_ROM_INT(LV_GRAD_DIR_NONE) },
    { MP_ROM_QSTR(MP_QSTR_GRAD_DIR_VER),    MP_ROM_INT(LV_GRAD_DIR_VER) },
    { MP_ROM_QSTR(MP_QSTR_GRAD_DIR_HOR),    MP_ROM_INT(LV_GRAD_DIR_HOR) },


    // enum lv_scrollbar_mode_t
    { MP_ROM_QSTR(MP_QSTR_SCROLLBAR_MODE_OFF),      MP_ROM_INT(LV_SCROLLBAR_MODE_OFF) },
    { MP_ROM_QSTR(MP_QSTR_SCROLLBAR_MODE_ON),       MP_ROM_INT(LV_SCROLLBAR_MODE_ON) },
    { MP_ROM_QSTR(MP_QSTR_SCROLLBAR_MODE_ACTIVE),   MP_ROM_INT(LV_SCROLLBAR_MODE_ACTIVE) },
    { MP_ROM_QSTR(MP_QSTR_SCROLLBAR_MODE_AUTO),     MP_ROM_INT(LV_SCROLLBAR_MODE_AUTO) },

    // enum lv_scroll_snap_t
    { MP_ROM_QSTR(MP_QSTR_SCROLL_SNAP_NONE),    MP_ROM_INT(LV_SCROLL_SNAP_NONE) },
    { MP_ROM_QSTR(MP_QSTR_SCROLL_SNAP_START),   MP_ROM_INT(LV_SCROLL_SNAP_START) },
    { MP_ROM_QSTR(MP_QSTR_SCROLL_SNAP_END),     MP_ROM_INT(LV_SCROLL_SNAP_END) },
    { MP_ROM_QSTR(MP_QSTR_SCROLL_SNAP_CENTER),  MP_ROM_INT(LV_SCROLL_SNAP_CENTER) },

    // enum lv_dir_t
    { MP_ROM_QSTR(MP_QSTR_DIR_NONE),    MP_ROM_INT(LV_DIR_NONE) },
    { MP_ROM_QSTR(MP_QSTR_DIR_LEFT),    MP_ROM_INT(LV_DIR_LEFT) },
    { MP_ROM_QSTR(MP_QSTR_DIR_RIGHT),   MP_ROM_INT(LV_DIR_RIGHT) },
    { MP_ROM_QSTR(MP_QSTR_DIR_TOP),     MP_ROM_INT(LV_DIR_TOP) },
    { MP_ROM_QSTR(MP_QSTR_DIR_BOTTOM),  MP_ROM_INT(LV_DIR_BOTTOM) },
    { MP_ROM_QSTR(MP_QSTR_DIR_HOR),     MP_ROM_INT(LV_DIR_HOR) },
    { MP_ROM_QSTR(MP_QSTR_DIR_VER),     MP_ROM_INT(LV_DIR_VER) },
    { MP_ROM_QSTR(MP_QSTR_DIR_ALL),     MP_ROM_INT(LV_DIR_ALL) },

    // enum lv_color_format_t
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_UNKNOWN),    MP_ROM_INT(LV_COLOR_FORMAT_UNKNOWN) },

    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_RAW),        MP_ROM_INT(LV_COLOR_FORMAT_RAW) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_RAW_ALPHA),  MP_ROM_INT(LV_COLOR_FORMAT_RAW_ALPHA) },
    /*<=1 byte (+alpha) formats*/
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_L8),         MP_ROM_INT(LV_COLOR_FORMAT_L8) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_I1),         MP_ROM_INT(LV_COLOR_FORMAT_I1) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_I2),         MP_ROM_INT(LV_COLOR_FORMAT_I2) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_I4),         MP_ROM_INT(LV_COLOR_FORMAT_I4) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_I8),         MP_ROM_INT(LV_COLOR_FORMAT_I8) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_A8),         MP_ROM_INT(LV_COLOR_FORMAT_A8) },

    /*2 byte (+alpha) formats*/
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_RGB565),     MP_ROM_INT(LV_COLOR_FORMAT_RGB565) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_RGB565A8),   MP_ROM_INT(LV_COLOR_FORMAT_RGB565A8) },

    /*3 byte (+alpha) formats*/
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_RGB888),     MP_ROM_INT(LV_COLOR_FORMAT_RGB888) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_ARGB8888),   MP_ROM_INT(LV_COLOR_FORMAT_ARGB8888) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_XRGB8888),   MP_ROM_INT(LV_COLOR_FORMAT_XRGB8888) },

    /*Formats not supported by software renderer but kept here so GPU can use it*/
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_A1),         MP_ROM_INT(LV_COLOR_FORMAT_A1) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_A2),         MP_ROM_INT(LV_COLOR_FORMAT_A2) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_A4),         MP_ROM_INT(LV_COLOR_FORMAT_A4) },

    /* reference to https://wiki.videolan.org/YUV/ */
    /*YUV planar formats*/
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_I420),       MP_ROM_INT(LV_COLOR_FORMAT_I420) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_I422),       MP_ROM_INT(LV_COLOR_FORMAT_I422) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_I444),       MP_ROM_INT(LV_COLOR_FORMAT_I444) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_I400),       MP_ROM_INT(LV_COLOR_FORMAT_I400) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_NV21),       MP_ROM_INT(LV_COLOR_FORMAT_NV21) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_NV12),       MP_ROM_INT(LV_COLOR_FORMAT_NV12) },

    /*YUV packed formats*/
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_YUY2),       MP_ROM_INT(LV_COLOR_FORMAT_YUY2) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_UYVY),       MP_ROM_INT(LV_COLOR_FORMAT_UYVY) },

    /*Color formats in which LVGL can render*/
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_NATIVE),    MP_ROM_INT(LV_COLOR_FORMAT_NATIVE) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FORMAT_NATIVE_WITH_ALPHA), MP_ROM_INT(LV_COLOR_FORMAT_NATIVE_WITH_ALPHA) },

};
STATIC MP_DEFINE_CONST_DICT(lvgl_module_globals, lvgl_module_globals_table);

const mp_obj_module_t lvgl_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&lvgl_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_lvgl, lvgl_module);
MP_REGISTER_OBJECT(lvgl_module);
