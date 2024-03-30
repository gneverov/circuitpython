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

typedef struct _machine_spi_obj_t {
    mp_obj_base_t base;
    spi_inst_t *const spi_inst;
    uint8_t spi_id;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bits;
    uint8_t firstbit;
    uint8_t sck;
    uint8_t mosi;
    uint8_t miso;
    uint32_t baudrate;
} machine_spi_obj_t;

STATIC mp_obj_t lvgl_ILI9341_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 3, 4, false);

    if (!mp_obj_is_exact_type(args[0], &machine_spi_type)) {
        mp_raise_TypeError(NULL);
    }
    machine_spi_obj_t *machine_spi = MP_OBJ_TO_PTR(args[0]);
    mp_hal_pin_obj_t cs_pin = mp_hal_get_pin_obj(args[1]);
    mp_hal_pin_obj_t dc_pin = mp_hal_get_pin_obj(args[2]);
    size_t buf_size = n_args > 3 ? mp_obj_get_int(args[3]) : 0;

    lvgl_lock_init();
    lv_ili9341_disp_t *drv = malloc(sizeof(lv_ili9341_disp_t));
    lv_display_t *disp;
    int errcode = lv_ili9341_disp_init(drv, machine_spi->spi_inst, cs_pin, dc_pin, &disp);
    lvgl_handle_display_t *handle = lvgl_handle_alloc_display(disp, &lvgl_type_ILI9341, lv_ili9341_disp_deinit);
    if (errcode) {
        goto cleanup;
    }
    if (!lvgl_handle_alloc_display_draw_buffers(handle, buf_size)) {
        errcode = MP_ENOMEM;
        goto cleanup;
    }
    lvgl_unlock();
    return MP_OBJ_FROM_PTR(lvgl_handle_get_display(handle));

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
