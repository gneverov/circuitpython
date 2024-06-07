// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./usb_cdc.h"
#if MICROPY_HW_USB_CDC

#include "FreeRTOS.h"

#include "tinyusb/cdc_device_cb.h"

#include "py/obj.h"
#include "py/runtime.h"
#include "py/stream_poll.h"


typedef struct {
    mp_obj_base_t base;
    int usb_itf;
    TickType_t timeout;
    mp_stream_poll_t poll;
} usb_cdc_obj_t;

static void usb_cdc_cb(void* context, tud_cdc_cb_type_t cb_type, tud_cdc_cb_args_t *cb_args) {
    usb_cdc_obj_t *self = context;
    mp_uint_t events = 0;
    switch (cb_type) {
        case TUD_CDC_RX:
            events = MP_STREAM_POLL_RD;
            break;
        case TUD_CDC_TX_COMPLETE:
            events = MP_STREAM_POLL_WR;
            break;
        default:
            events = MP_STREAM_POLL_ERR;
            break;
    }
    mp_stream_poll_signal(&self->poll, events, NULL);
}

static mp_obj_t usb_cdc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);
    mp_int_t usb_itf = mp_obj_get_int(args[0]);

    usb_cdc_obj_t *self = mp_obj_malloc_with_finaliser(usb_cdc_obj_t, type);
    self->usb_itf = -1;
    self->timeout = portMAX_DELAY;
    mp_stream_poll_init(&self->poll);

    if (!tud_cdc_set_cb(usb_itf, usb_cdc_cb, self)) {
        mp_raise_OSError(MP_EBUSY);
    }
    self->usb_itf = usb_itf;

    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t usb_cdc_del(mp_obj_t self_in) {
    usb_cdc_obj_t* self = MP_OBJ_TO_PTR(self_in);
    if (self->usb_itf != -1) {
        tud_cdc_clear_cb(self->usb_itf);
        self->usb_itf = -1;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(usb_cdc_del_obj, usb_cdc_del);

static void usb_cdc_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    usb_cdc_obj_t* self = MP_OBJ_TO_PTR(self_in);
    if ((attr == MP_QSTR_connected) && (dest[0] != MP_OBJ_SENTINEL)) {
        dest[0] = mp_obj_new_bool(tud_cdc_n_connected(self->usb_itf));
    }
    else {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

static mp_uint_t usb_cdc_close(mp_obj_t self_in, int *errcode) {
    usb_cdc_obj_t* self = MP_OBJ_TO_PTR(self_in);
    mp_stream_poll_close(&self->poll);
    usb_cdc_del(self_in);
    return 0;
}

static mp_uint_t usb_cdc_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    usb_cdc_obj_t* self = MP_OBJ_TO_PTR(self_in);
    if (self->usb_itf == -1) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    uint32_t br = tud_cdc_n_read(self->usb_itf, buf, size);
    if (br == 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }

    return br;
}

static mp_uint_t usb_cdc_read_blocking(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    usb_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_poll_block(self_in, buf, size, errcode, usb_cdc_read, MP_STREAM_POLL_RD, self->timeout, false);
}

static mp_uint_t usb_cdc_write(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    usb_cdc_obj_t* self = MP_OBJ_TO_PTR(self_in);
    if (self->usb_itf == -1) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    uint32_t bw = tud_cdc_n_write(self->usb_itf, buf, size);
    if (bw == 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }       
    return bw;
}

static mp_uint_t usb_cdc_write_blocking(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    usb_cdc_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_poll_block(self_in, (void*)buf, size, errcode, usb_cdc_write, MP_STREAM_POLL_WR, self->timeout, true);
}

static mp_uint_t usb_cdc_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    usb_cdc_obj_t* self = MP_OBJ_TO_PTR(self_in);
    if ((self->usb_itf == -1) && (request != MP_STREAM_CLOSE)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    switch (request) {
        case MP_STREAM_FLUSH:
            tud_cdc_n_write_flush(self->usb_itf);
            return 0;
        case MP_STREAM_SEEK:
            *errcode = MP_ESPIPE;
            return MP_STREAM_ERROR;
        case MP_STREAM_TIMEOUT:
            return mp_stream_timeout(&self->timeout, arg, errcode);            
        case MP_STREAM_POLL_CTL:
            return mp_stream_poll_ctl(&self->poll, (void*)arg, errcode);
        case MP_STREAM_CLOSE:
            return usb_cdc_close(self, errcode);
        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

static const mp_rom_map_elem_t usb_cdc_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),       MP_ROM_QSTR(MP_QSTR_CdcDevice) },
    { MP_ROM_QSTR(MP_QSTR___del__),        MP_ROM_PTR(&usb_cdc_del_obj) },    
    { MP_ROM_QSTR(MP_QSTR_close),          MP_ROM_PTR(&mp_stream_close_obj) },  
    { MP_ROM_QSTR(MP_QSTR_read),           MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),       MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),          MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout),     MP_ROM_PTR(&mp_stream_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),          MP_ROM_PTR(&mp_stream_flush_obj) },
};
static MP_DEFINE_CONST_DICT(usb_cdc_locals_dict, usb_cdc_locals_dict_table);

static const mp_stream_p_t usb_cdc_stream_p = {
    .read = usb_cdc_read_blocking,
    .write = usb_cdc_write_blocking,
    .ioctl = usb_cdc_ioctl,
    .can_poll = 1,
};

MP_DEFINE_CONST_OBJ_TYPE(
    usb_cdc_type,
    MP_QSTR_CdcDevice,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, usb_cdc_make_new,
    attr, usb_cdc_attr,
    protocol, &usb_cdc_stream_p,
    locals_dict, &usb_cdc_locals_dict
    );
#endif