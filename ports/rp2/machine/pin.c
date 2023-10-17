// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "pico/gpio.h"

#include "./pin.h"
#include "py/parseargs.h"


enum gpio_irq_pulse {
    GPIO_IRQ_PULSE_DOWN = 0x10u,
    GPIO_IRQ_PULSE_UP = 0x20u,
};

STATIC void pin_enable_interrupt(pin_obj_t *self) {
    uint32_t event_mask = self->event_mask & 0xf;
    if (self->event_mask & 0x30) {
        event_mask |= GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE;
    }
    gpio_set_irq_enabled(self->pin, event_mask, true);
}

STATIC void pin_disable_interrupt(pin_obj_t *self) {
    gpio_set_irq_enabled(self->pin, 0xf, false);
}

STATIC void pin_irq_handler(uint gpio, uint32_t events, void *context) {
    pin_obj_t *self = context;
    self->int_count++;

    uint64_t now = time_us_64();
    if (self->event_mask & GPIO_IRQ_PULSE_DOWN) {
        if ((events & GPIO_IRQ_EDGE_FALL) && (self->pulse_down >= 0)) {
            self->pulse_down = -now;
        }
        if ((events & GPIO_IRQ_EDGE_RISE) && (self->pulse_down < 0)) {
            self->pulse_down += now;
            events |= GPIO_IRQ_PULSE_DOWN;
        }
    }
    if (self->event_mask & GPIO_IRQ_PULSE_UP) {
        if ((events & GPIO_IRQ_EDGE_RISE) && (self->pulse_up >= 0)) {
            self->pulse_down = -now;
        }
        if ((events & GPIO_IRQ_EDGE_FALL) && (self->pulse_up < 0)) {
            self->pulse_down += now;
            events |= GPIO_IRQ_PULSE_UP;
        }
    }
    if (self->event_mask & (GPIO_IRQ_PULSE_DOWN | GPIO_IRQ_PULSE_UP)) {
        events &= ~(GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE);
    }
    self->events |= events;
    self->event_mask &= ~events;

    pin_disable_interrupt(self);
    pin_enable_interrupt(self);

    BaseType_t xHigherPriorityTaskWoken = 0;
    mp_stream_poll_signal(&self->poll, MP_STREAM_POLL_RD, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void pin_init(pin_obj_t *self, const mp_obj_type_t *type) {
    self->base.type = type;
    self->pin = -1u;
    mp_stream_poll_init(&self->poll);
    self->timeout = portMAX_DELAY;
    self->events = 0;
    self->int_count = 0;
}

void pin_deinit(pin_obj_t *self) {
    if (self->pin != -1) {
        pico_gpio_clear_irq(self->pin);
        gpio_deinit(self->pin);
        self->pin = -1;
    }
}

bool pin_inited(pin_obj_t *self) {
    return self->pin != -1u;
}

pin_obj_t *pin_get_raise(mp_obj_t self_in) {
    pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!pin_inited(self)) {
        mp_raise_OSError(MP_EBADF);
    }
    return self;
}

STATIC mp_obj_t pin_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_, MP_QSTR_pin, 0 };
    mp_hal_pin_obj_t pin;
    parse_args_and_kw(n_args, n_kw, args, "O&", kws, mp_hal_get_pin_obj, &pin);

    pin_obj_t *self = m_new_obj_with_finaliser(pin_obj_t);
    pin_init(self, type);
    self->pin = pin;

    gpio_init(pin);

    pico_gpio_set_irq(self->pin, pin_irq_handler, self);

    return MP_OBJ_FROM_PTR(self);
}

mp_uint_t pin_close(mp_obj_t self_in, int *errcode) {
    pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (pin_inited(self)) {
        mp_stream_poll_close(&self->poll);
    }
    pin_deinit(self);
    return 0;
}

STATIC mp_obj_t pin_del(mp_obj_t self_in) {
    pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    pin_deinit(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pin_del_obj, pin_del);

STATIC mp_obj_t pin_set_pulls(size_t n_args, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_pin_mask, MP_QSTR_pull_up, MP_QSTR_pull_down, 0 };
    mp_obj_t self_in;
    mp_int_t pull_up = 0, pull_down = 0;
    parse_args_and_kw(n_args, 0, args, "O|pp", kws, &self_in, &pull_up, &pull_down);

    pin_obj_t *self = pin_get_raise(self_in);
    gpio_set_pulls(self->pin, pull_up, pull_down);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(pin_set_pulls_obj, 1, 3, pin_set_pulls);

STATIC void pin_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    switch (attr) {
        case MP_QSTR_value: {
            pin_obj_t *self = pin_get_raise(self_in);
            if (dest[0] == MP_OBJ_SENTINEL) {
                // store attr
                if (dest[1] == mp_const_none) {
                    gpio_set_dir(self->pin, false);
                    dest[0] = MP_OBJ_NULL;
                } else if (dest[1] != MP_OBJ_NULL) {
                    bool value = mp_obj_is_true(dest[1]);
                    gpio_set_dir(self->pin, true);
                    gpio_put(self->pin, value);
                    dest[0] = MP_OBJ_NULL;
                }
            } else {
                // load attr
                dest[0] = mp_obj_new_bool(gpio_get(self->pin));
            }
            break;
        }
        default:
            dest[1] = MP_OBJ_SENTINEL;
    }
}

STATIC mp_uint_t pin_wait_nonblock(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint32_t event = *(uint32_t *)buf;
    if (!pin_inited(self)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    pin_disable_interrupt(self);
    mp_uint_t ret = 0;
    if (self->events & event) {
        self->events &= ~event;
        if (event & GPIO_IRQ_PULSE_DOWN) {
            ret = self->pulse_down;
        }
        if (event & GPIO_IRQ_PULSE_UP) {
            ret = self->pulse_up;
        }
    } else {
        self->event_mask |= event;
        *errcode = MP_EAGAIN;
        ret = MP_STREAM_ERROR;
    }
    pin_enable_interrupt(self);
    return ret;
}

STATIC mp_obj_t pin_wait(mp_obj_t self_in, mp_obj_t event_in) {
    pin_obj_t *self = pin_get_raise(self_in);
    uint32_t event = mp_obj_get_int(event_in);
    if (event & (event - 1)) {
        mp_raise_ValueError(NULL);
    }

    int errcode;
    mp_uint_t ret = mp_poll_block(self_in, &event, sizeof(event), &errcode, pin_wait_nonblock, MP_STREAM_POLL_RD, self->timeout, false);
    return mp_stream_return(ret, errcode);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(pin_wait_obj, pin_wait);

STATIC mp_uint_t pin_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (!pin_inited(self) && (request != MP_STREAM_CLOSE)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }
    mp_uint_t ret = 0;
    switch (request) {
        case MP_STREAM_CLOSE:
            ret = pin_close(self_in, errcode);
            break;
        case MP_STREAM_TIMEOUT:
            return mp_stream_timeout(&self->timeout, arg, errcode);
        case MP_STREAM_POLL_CTL:
            pin_disable_interrupt(self);
            ret = mp_stream_poll_ctl(&self->poll, (void *)arg, errcode);
            pin_enable_interrupt(self);
            break;
        default:
            *errcode = MP_EINVAL;
            ret = MP_STREAM_ERROR;
            break;
    }
    return ret;
}
#ifndef NDEBUG
#include <stdio.h>

STATIC mp_obj_t pin_debug(mp_obj_t self_in) {
    pin_obj_t *self = MP_OBJ_TO_PTR(self_in);
    printf("pin %p\n", self);
    printf("  events:      0x%02lx\n", self->events);
    printf("  int_count:   %d\n", self->int_count);

    if (self->pin != -1u) {
        pico_gpio_debug(self->pin);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(pin_debug_obj, pin_debug);
#endif

STATIC const mp_rom_map_elem_t pin_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&pin_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_pulls),       MP_ROM_PTR(&pin_set_pulls_obj) },

    { MP_ROM_QSTR(MP_QSTR_read),            MP_ROM_PTR(&mp_stream_read1_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),        MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout),      MP_ROM_PTR(&mp_stream_settimeout_obj) },

    { MP_ROM_QSTR(MP_QSTR_wait),            MP_ROM_PTR(&pin_wait_obj) },
    { MP_ROM_QSTR(MP_QSTR_LEVEL_LOW),       MP_ROM_INT(GPIO_IRQ_LEVEL_LOW) },
    { MP_ROM_QSTR(MP_QSTR_LEVEL_HIGH),      MP_ROM_INT(GPIO_IRQ_LEVEL_HIGH) },
    { MP_ROM_QSTR(MP_QSTR_EDGE_FALL),       MP_ROM_INT(GPIO_IRQ_EDGE_FALL) },
    { MP_ROM_QSTR(MP_QSTR_EDGE_RISE),       MP_ROM_INT(GPIO_IRQ_EDGE_RISE) },
    { MP_ROM_QSTR(MP_QSTR_PULSE_DOWN),      MP_ROM_INT(GPIO_IRQ_PULSE_DOWN) },
    { MP_ROM_QSTR(MP_QSTR_PULSE_UP),        MP_ROM_INT(GPIO_IRQ_PULSE_UP) },

    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_debug),           MP_ROM_PTR(&pin_debug_obj) },
    #endif
};
STATIC MP_DEFINE_CONST_DICT(pin_locals_dict, pin_locals_dict_table);

STATIC const mp_stream_p_t pin_stream_p = {
    .ioctl = pin_ioctl,
    .is_text = 0,
    .can_poll = 1,
};

MP_DEFINE_CONST_OBJ_TYPE(
    pin_type,
    MP_QSTR_Pin,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, pin_make_new,
    attr, pin_attr,
    protocol, &pin_stream_p,
    locals_dict, &pin_locals_dict
    );
