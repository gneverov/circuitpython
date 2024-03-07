// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./usb_msc.h"
#if MICROPY_HW_USB_MSC
#include "tinyusb/msc_device.h"

#include "py/stream_poll.h"

STATIC mp_obj_t usb_msc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    mp_obj_base_t *self = m_new_obj(mp_obj_base_t);
    self->type = type;
    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t usb_msc_insert(size_t n_args, const mp_obj_t *args) {
    const char *device = mp_obj_str_get_str(args[1]);
    bool readonly = n_args > 2 ? mp_obj_is_true(args[2]) : false;
    int ret = tud_msc_insert(0, device, readonly);
    return mp_stream_return(ret, errno);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(usb_msc_insert_obj, 2, 3, usb_msc_insert);

STATIC mp_obj_t usb_msc_eject(mp_obj_t self_in) {
    int ret = tud_msc_eject(0);
    return mp_stream_return(ret, errno);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(usb_msc_eject_obj, usb_msc_eject);

STATIC mp_obj_t usb_msc_ready(mp_obj_t self_in) {
    int ret = tud_msc_ready(0);
    return mp_obj_new_bool(ret);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(usb_msc_ready_obj, usb_msc_ready);

STATIC const mp_rom_map_elem_t usb_msc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_MscDevice) },
    { MP_ROM_QSTR(MP_QSTR_insert),          MP_ROM_PTR(&usb_msc_insert_obj) },    
    { MP_ROM_QSTR(MP_QSTR_eject),           MP_ROM_PTR(&usb_msc_eject_obj) }, 
    { MP_ROM_QSTR(MP_QSTR_ready),           MP_ROM_PTR(&usb_msc_ready_obj) }, 
};
STATIC MP_DEFINE_CONST_DICT(usb_msc_locals_dict, usb_msc_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    usb_msc_type,
    MP_QSTR_MscDevice,
    MP_TYPE_FLAG_NONE,
    make_new, usb_msc_make_new,
    locals_dict, &usb_msc_locals_dict
    );
#endif
