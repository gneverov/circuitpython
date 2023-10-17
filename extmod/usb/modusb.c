// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "tusb.h"
#include "device/dcd.h"

#include "tinyusb/tusb_lock.h"

#include "./usb_cdc.h"
#include "./usb_config.h"
#include "py/mperrno.h"
#include "py/runtime.h"


STATIC mp_obj_t usb_speed(void) {
    mp_float_t values[] = { 12.0e6f, 1.5e6f, 480.0e6f };
    tud_lock();
    int index = tud_speed_get();
    tud_unlock();
    return mp_obj_new_float(index < 3 ? values[index] : 0.0f);
}
MP_DEFINE_CONST_FUN_OBJ_0(usb_speed_obj, usb_speed);

STATIC mp_obj_t usb_connected(void) {
    tud_lock();
    bool connected = tud_connected();
    tud_unlock();
    return mp_obj_new_bool(connected);
}
MP_DEFINE_CONST_FUN_OBJ_0(usb_connected_obj, usb_connected);

STATIC mp_obj_t usb_mounted(void) {
    tud_lock();
    bool mounted = tud_mounted();
    tud_unlock();
    return mp_obj_new_bool(mounted);
}
MP_DEFINE_CONST_FUN_OBJ_0(usb_mounted_obj, usb_mounted);

STATIC mp_obj_t usb_suspended(void) {
    tud_lock();
    bool suspended = tud_suspended();
    tud_unlock();
    return mp_obj_new_bool(suspended);
}
MP_DEFINE_CONST_FUN_OBJ_0(usb_suspended_obj, usb_suspended);

STATIC mp_obj_t usb_connect(void) {
    tud_lock();
    bool ok = tud_connect();
    tud_unlock();
    if (!ok) {
        mp_raise_OSError(MP_EPERM);
    }    
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(usb_connect_obj, usb_connect);

STATIC mp_obj_t usb_disconnect(void) {
    tud_lock();
    dcd_event_bus_signal(0, DCD_EVENT_UNPLUGGED, false);
    bool ok = tud_disconnect();
    tud_unlock();
    if (!ok) {
        mp_raise_OSError(MP_EPERM);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_0(usb_disconnect_obj, usb_disconnect);

STATIC const mp_rom_map_elem_t usb_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_usb) },
    { MP_ROM_QSTR(MP_QSTR_speed),           MP_ROM_PTR(&usb_speed_obj) },    
    { MP_ROM_QSTR(MP_QSTR_connected),       MP_ROM_PTR(&usb_connected_obj) },
    { MP_ROM_QSTR(MP_QSTR_mounted),         MP_ROM_PTR(&usb_mounted_obj) },
    { MP_ROM_QSTR(MP_QSTR_suspended),       MP_ROM_PTR(&usb_suspended_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect),         MP_ROM_PTR(&usb_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_disconnect),      MP_ROM_PTR(&usb_disconnect_obj) },

#if CFG_TUD_CDC
    { MP_ROM_QSTR(MP_QSTR_UsbCdcDevice),    MP_ROM_PTR(&usb_cdc_type) },
#endif

    { MP_ROM_QSTR(MP_QSTR_UsbConfig),       MP_ROM_PTR(&usb_config_type) },
};
STATIC MP_DEFINE_CONST_DICT(usb_module_globals, usb_module_globals_table);

const mp_obj_module_t usb_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&usb_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_usb, usb_module);
