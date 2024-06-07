// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "./indev.h"
#include "./ft6206.h"
#include "./modlvgl.h"
#include "./drivers/lv_ft6206_indev.h"

#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"


extern const mp_obj_type_t machine_i2c_type;

typedef struct _machine_i2c_obj_t {
    mp_obj_base_t base;
    i2c_inst_t *const i2c_inst;
    uint8_t i2c_id;
    uint8_t scl;
    uint8_t sda;
    uint32_t freq;
    uint32_t timeout;
} machine_i2c_obj_t;

static mp_obj_t lvgl_FT6206_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 2, 2, false);

    if (!mp_obj_is_exact_type(args[0], &machine_i2c_type)) {
        mp_raise_TypeError(NULL);
    }
    machine_i2c_obj_t *machine_i2c = MP_OBJ_TO_PTR(args[0]);
    mp_int_t trig = mp_hal_get_pin_obj(args[1]);

    lvgl_lock_init();
    lv_ft6206_indev_t *drv = malloc(sizeof(lv_ft6206_indev_t));
    lv_indev_t *indev;
    int errcode = lv_ft6206_indev_init(drv, machine_i2c->i2c_inst, trig, machine_i2c->timeout, &indev);
    lvgl_indev_handle_t *handle = lvgl_indev_alloc_handle(indev, lv_ft6206_indev_deinit);
    if (errcode) {
        goto cleanup;
    }
    return lvgl_unlock_ptr(&handle->base);

cleanup:
    lv_indev_delete(indev);
    lvgl_unlock();
    mp_raise_OSError(errcode);    
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_FT6206,
    MP_ROM_QSTR_CONST(MP_QSTR_FT6206),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_FT6206_make_new,
    parent, &lvgl_type_indev
    );
MP_REGISTER_OBJECT(lvgl_type_FT6206);
