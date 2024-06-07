// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>

#include "hardware/spi.h"

#include "./display.h"
#include "./ili9341.h"
#include "./modlvgl.h"
#include "./obj.h"
#include "./drivers/lv_ili9341_disp.h"

#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"


extern const mp_obj_type_t machine_spi_type;

static mp_obj_t lvgl_ILI9341_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 4, 5, false);

    uint spi_index = mp_obj_get_int(args[0]);
    mp_hal_pin_obj_t cs_pin = mp_hal_get_pin_obj(args[1]);
    mp_hal_pin_obj_t dc_pin = mp_hal_get_pin_obj(args[2]);
    uint baudrate = mp_obj_get_int(args[3]);
    size_t buf_size = n_args > 4 ? mp_obj_get_int(args[4]) : 0;
    if (spi_index >= NUM_SPIS) { 
        mp_raise_ValueError(NULL);
    }

    lvgl_lock_init();
    lv_ili9341_disp_t *drv = malloc(sizeof(lv_ili9341_disp_t));
    lv_display_t *disp;
    int errcode = lv_ili9341_disp_init(drv, &pico_spis_ll[spi_index], cs_pin, dc_pin, baudrate, &disp);
    lvgl_display_handle_t *handle = lvgl_display_alloc_handle(disp, lv_ili9341_disp_deinit);
    if (errcode) {
        goto cleanup;
    }
    if (!lvgl_display_alloc_draw_buffers(handle, buf_size)) {
        errcode = MP_ENOMEM;
        goto cleanup;
    }
    return lvgl_unlock_ptr(&handle->base);

cleanup:
    lv_display_delete(disp);
    lvgl_unlock();
    mp_raise_OSError(errcode);
}

MP_DEFINE_CONST_OBJ_TYPE(
    lvgl_type_ILI9341,
    MP_ROM_QSTR_CONST(MP_QSTR_ILI9341),
    MP_TYPE_FLAG_NONE,
    make_new, lvgl_ILI9341_make_new,
    attr, lvgl_display_attr,
    parent, &lvgl_type_display
    );
MP_REGISTER_OBJECT(lvgl_type_ILI9341);
