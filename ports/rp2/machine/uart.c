// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "FreeRTOS.h"

#include "py/obj.h"
#include "py/parseargs.h"
#include "py/runtime.h"
#include "machine_pin.h"
#include "./uart.h"


static void uart_handler(pico_uart_t *uart, uint events) {
    uart_obj_t *self = (uart_obj_t *)((uint8_t *)uart - offsetof(uart_obj_t, uart));
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    mp_stream_poll_signal(&self->poll, events, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

STATIC mp_obj_t uart_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_, MP_QSTR_baudrate, 0 };
    mp_hal_pin_obj_t rx_pin, tx_pin;
    mp_uint_t baudrate = 115200;
    parse_args_and_kw(n_args, n_kw, args, "O&O&|i", kws, mp_hal_get_pin_obj, &rx_pin, mp_hal_get_pin_obj, &tx_pin, &baudrate);

    if (((tx_pin ^ rx_pin) & ~3) || ((tx_pin & 3) != 0) || ((rx_pin & 3) != 1)) {
        mp_raise_ValueError(MP_ERROR_TEXT("invalid pins"));
    }

    uart_obj_t *self = m_new_obj_with_finaliser(uart_obj_t);
    self->base.type = type;
    self->timeout = portMAX_DELAY;
    mp_stream_poll_init(&self->poll);

    uart_inst_t *hw_uart = (tx_pin + 4) & 8 ? uart1 : uart0;
    self->uart_num = uart_get_index(hw_uart);
    if (!pico_uart_init(&self->uart, hw_uart, tx_pin, rx_pin, baudrate, uart_handler)) {
        mp_raise_OSError(errno);
    }

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t uart_del(mp_obj_t self_in) {
    uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->uart_num != -1) {
        pico_uart_deinit(&self->uart);
        self->uart_num = -1;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(uart_del_obj, uart_del);

STATIC mp_uint_t uart_close(mp_obj_t self_in, int *errcode) {
    uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_stream_poll_close(&self->poll);
    uart_del(self_in);
    return 0;
}

STATIC mp_uint_t uart_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->uart_num == -1) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    uint32_t br = pico_uart_read(&self->uart, buf, size);
    if (br == 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }

    return br;
}

STATIC mp_uint_t machine_uart_read_blocking(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_poll_block(self_in, buf, size, errcode, uart_read, MP_STREAM_POLL_RD, self->timeout, false);
}

STATIC mp_uint_t uart_write(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->uart_num == -1) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    uint32_t bw = pico_uart_write(&self->uart, buf, size);
    if (bw == 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    return bw;
}

STATIC mp_uint_t machine_uart_write_blocking(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_poll_block(self_in, (void *)buf, size, errcode, uart_write, MP_STREAM_POLL_WR, self->timeout, true);
}

STATIC mp_uint_t uart_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    uart_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if ((self->uart_num == -1) && (request != MP_STREAM_CLOSE)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    switch (request) {
        case MP_STREAM_FLUSH:
            return 0;
        case MP_STREAM_SEEK:
            *errcode = MP_ESPIPE;
            return MP_STREAM_ERROR;
        case MP_STREAM_TIMEOUT:
            return mp_stream_timeout(&self->timeout, arg, errcode);
        case MP_STREAM_POLL_CTL:
            return mp_stream_poll_ctl(&self->poll, (void *)arg, errcode);
        case MP_STREAM_CLOSE:
            return uart_close(self, errcode);
        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

STATIC const mp_rom_map_elem_t uart_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),       MP_ROM_QSTR(MP_QSTR_CdcDevice) },
    { MP_ROM_QSTR(MP_QSTR___del__),        MP_ROM_PTR(&uart_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),          MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_read),           MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),       MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),          MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout),     MP_ROM_PTR(&mp_stream_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),          MP_ROM_PTR(&mp_stream_flush_obj) },
};
STATIC MP_DEFINE_CONST_DICT(uart_locals_dict, uart_locals_dict_table);

STATIC const mp_stream_p_t uart_stream_p = {
    .read = machine_uart_read_blocking,
    .write = machine_uart_write_blocking,
    .ioctl = uart_ioctl,
    .can_poll = 1,
};

MP_DEFINE_CONST_OBJ_TYPE(
    uart_type,
    MP_QSTR_UART,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, uart_make_new,
    protocol, &uart_stream_p,
    locals_dict, &uart_locals_dict
    );
