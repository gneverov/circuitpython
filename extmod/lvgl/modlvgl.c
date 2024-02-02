// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include "./modlvgl.h"
#include "./display.h"
#include "./indev.h"
#include "./obj.h"
#include "./queue.h"
#include "./style.h"
#include "./widgets.h"

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
    while (!lvgl_exit) {
        uint32_t sleep_ms = lv_task_handler();
        TickType_t sleep_ticks = sleep_ms == LV_NO_TIMER_READY ? portMAX_DELAY : pdMS_TO_TICKS(sleep_ms);
        xSemaphoreGive(lvgl_mutex);
        ulTaskNotifyTake(pdTRUE, sleep_ticks);
        xSemaphoreTake(lvgl_mutex, portMAX_DELAY);        
    }

    lvgl_queue_default->writer_closed = 1;
    lvgl_queue_free(lvgl_queue_default);
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
    lvgl_queue_t *queue = lvgl_queue_copy(lvgl_queue_default);
    lvgl_unlock();
    mp_obj_t obj = lvgl_queue_from(queue);
    lvgl_lock();
    lvgl_queue_free(queue);
    queue = NULL;
    lvgl_unlock();

    if (obj == mp_const_none) {
        return mp_const_none;
    }

    mp_obj_t args[2];
    mp_load_method(obj, MP_QSTR_run, args);

    while(MP_OBJ_SMALL_INT_VALUE(mp_call_method_n_kw(0, 0, args)) > 0);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(lvgl_run_forever_obj, lvgl_run_forever);

STATIC mp_obj_t lvgl_getattr(mp_obj_t attr) {
    switch (MP_OBJ_QSTR_VALUE(attr)) {
        case MP_QSTR_display:        
            return lvgl_display_get_default();
        default:
            return MP_OBJ_NULL;
    }
}
MP_DEFINE_CONST_FUN_OBJ_1(lvgl_getattr_obj, lvgl_getattr);

STATIC const mp_rom_map_elem_t lvgl_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_lvgl) },
    { MP_ROM_QSTR(MP_QSTR___getattr__),     MP_ROM_PTR(&lvgl_getattr_obj) },
    { MP_ROM_QSTR(MP_QSTR_init),            MP_ROM_PTR(&lvgl_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit),          MP_ROM_PTR(&lvgl_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_run_forever),     MP_ROM_PTR(&lvgl_run_forever_obj) },
    // { MP_ROM_QSTR(MP_QSTR_Class),           MP_ROM_PTR(&lvgl_type_class) },
    { MP_ROM_QSTR(MP_QSTR_Button),          MP_ROM_PTR(&lvgl_type_button) },
    { MP_ROM_QSTR(MP_QSTR_Display),         MP_ROM_PTR(&lvgl_type_display) },
    { MP_ROM_QSTR(MP_QSTR_InDev),           MP_ROM_PTR(&lvgl_type_indev) },
    { MP_ROM_QSTR(MP_QSTR_Label),           MP_ROM_PTR(&lvgl_type_label) },
    { MP_ROM_QSTR(MP_QSTR_Object),          MP_ROM_PTR(&lvgl_type_obj) },
    { MP_ROM_QSTR(MP_QSTR_ObjectCollection), MP_ROM_PTR(&lvgl_type_obj_list) },
    { MP_ROM_QSTR(MP_QSTR_Slider),          MP_ROM_PTR(&lvgl_type_slider) },

    { MP_ROM_QSTR(MP_QSTR_FT6206),          MP_ROM_PTR(&lvgl_type_FT6206) },
    { MP_ROM_QSTR(MP_QSTR_ILI9341),         MP_ROM_PTR(&lvgl_type_ILI9341) },


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
};
STATIC MP_DEFINE_CONST_DICT(lvgl_module_globals, lvgl_module_globals_table);

const mp_obj_module_t lvgl_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&lvgl_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_lvgl, lvgl_module);
