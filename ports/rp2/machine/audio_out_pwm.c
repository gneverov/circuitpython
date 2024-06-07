// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <memory.h>

#include "hardware/pwm.h"
#include "pico/divider.h"
#include "pico/rand.h"

#include "pico/dma.h"
#include "pico/pwm.h"

#include "./audio_out_pwm.h"
#include "machine_pin.h"
#include "py/binary.h"
#include "py/mperrno.h"
#include "py/objarray.h"
#include "py/parseargs.h"
#include "py/runtime.h"
#include "py/stream.h"


static void audio_out_pwm_init(audio_out_pwm_obj_t *self) {
    self->a_pin = -1u;
    self->b_pin = -1u;
    self->pwm_slice = -1u;
    pico_fifo_init(&self->fifo, true);
    self->error = 0;
    mp_stream_poll_init(&self->poll);
    self->timeout = portMAX_DELAY;
    self->fragment[3] = 0;
}

static void audio_out_pwm_deinit(audio_out_pwm_obj_t *self) {
    pico_fifo_deinit(&self->fifo);

    if (self->pwm_slice != -1u) {
        gpio_deinit(self->a_pin);
        gpio_deinit(self->b_pin);
        pwm_config c = pwm_get_default_config();
        pwm_init(self->pwm_slice, &c, false);
        self->pwm_slice = -1u;
    }
}

static bool audio_out_pwm_inited(audio_out_pwm_obj_t *self) {
    return self->pwm_slice != -1u;
}

static audio_out_pwm_obj_t *audio_out_pwm_get(mp_obj_t self_in) {
    return MP_OBJ_TO_PTR(mp_obj_cast_to_native_base(self_in, MP_OBJ_FROM_PTR(&audio_out_pwm_type)));
}

static audio_out_pwm_obj_t *audio_out_pwm_get_raise(mp_obj_t self_in) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get(self_in);
    if (!audio_out_pwm_inited(self)) {
        mp_raise_OSError(MP_EBADF);
    }
    return self;
}

static void audio_out_pwm_irq_handler(pico_fifo_t *fifo, bool stalled) {
    audio_out_pwm_obj_t *self = (audio_out_pwm_obj_t *)((uint8_t *)fifo - offsetof(audio_out_pwm_obj_t, fifo));
    self->int_count++;

    BaseType_t xHigherPriorityTaskWoken = 0;
    mp_stream_poll_signal(&self->poll, MP_STREAM_POLL_WR, &xHigherPriorityTaskWoken);
    if (stalled) {
        pwm_set_both_levels(self->pwm_slice, self->top / 2, self->top / 2);
        self->error = 0;
        self->stalls++;
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static mp_obj_t audio_out_pwm_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_, MP_QSTR_, MP_QSTR_num_channels, MP_QSTR_sample_rate, MP_QSTR_bytes_per_sample, MP_QSTR_fifo_size, MP_QSTR_threshold, MP_QSTR_pwm_bits, MP_QSTR_phase_correct, 0 };
    mp_hal_pin_obj_t a_pin, b_pin;
    mp_int_t num_channels, sample_rate, bytes_per_sample;
    mp_int_t fifo_size = 1024, threshold = 256;
    mp_int_t pwm_bits = 10, phase_correct = 0;
    parse_args_and_kw(n_args, n_kw, args, "O&O&iii|ii$ii", kws, mp_hal_get_pin_obj, &a_pin, mp_hal_get_pin_obj, &b_pin, &num_channels, &sample_rate, &bytes_per_sample, &fifo_size, &threshold, &pwm_bits, &phase_correct);

    if (a_pin == b_pin) {
        mp_raise_ValueError("Pins must be different");
    }

    uint pwm_slice = pwm_gpio_to_slice_num(a_pin);
    if (pwm_slice != pwm_gpio_to_slice_num(b_pin)) {
        mp_raise_ValueError("Pins must share PWM slice");
    }

    int errcode = 0;
    audio_out_pwm_obj_t *self = mp_obj_malloc_with_finaliser(audio_out_pwm_obj_t, type);
    audio_out_pwm_init(self);
    self->a_pin = a_pin;
    self->b_pin = b_pin;
    self->pwm_slice = pwm_slice;

    self->top = (clock_get_hz(clk_sys) + (sample_rate / 2)) / sample_rate;
    if (phase_correct) {
        self->top = (self->top + 1) / 2;
    }
    self->divisor = (0x10000 << pwm_bits) / self->top;

    uint dreq = pwm_get_dreq(pwm_slice);

    if (!pico_fifo_alloc(&self->fifo, fifo_size, dreq, threshold, DMA_SIZE_16, false, &pwm_hw->slice[pwm_slice].cc)) {
        errcode = errno;
        goto _finally;
    }
    pico_fifo_set_enabled(&self->fifo, false);

    pwm_config c = pwm_get_default_config();
    pwm_config_set_phase_correct(&c, phase_correct);
    pwm_config_set_wrap(&c, self->top - 1);
    pwm_init(pwm_slice, &c, false);

    pwm_set_both_levels(pwm_slice, self->top / 2, self->top / 2);
    gpio_set_function(a_pin, GPIO_FUNC_PWM);
    gpio_set_function(b_pin, GPIO_FUNC_PWM);

    pwm_set_enabled(pwm_slice, true);
    pwm_set_output_polarity(pwm_slice, false, true);

    self->num_channels = num_channels;
    self->sample_rate = sample_rate;
    self->bytes_per_sample = bytes_per_sample;
    self->pwm_bits = pwm_bits;

    pico_fifo_set_handler(&self->fifo, audio_out_pwm_irq_handler);

_finally:
    if (errcode) {
        audio_out_pwm_deinit(self);
        mp_raise_OSError(errcode);
    }
    return MP_OBJ_FROM_PTR(self);
}

mp_uint_t audio_out_pwm_close(mp_obj_t self_in, int *errcode) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get(self_in);
    if (audio_out_pwm_inited(self)) {
        pico_fifo_clear(&self->fifo);
        mp_stream_poll_close(&self->poll);
    }
    audio_out_pwm_deinit(self);
    return 0;
}

static mp_obj_t audio_out_pwm_del(mp_obj_t self_in) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get(self_in);
    audio_out_pwm_deinit(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(audio_out_pwm_del_obj, audio_out_pwm_del);

static size_t audio_out_pwm_transcode(audio_out_pwm_obj_t *self, uint16_t *out_buffer, size_t out_size, const uint8_t *in_buffer, size_t in_size) {
    const size_t out_bytes_per_sample = sizeof(uint16_t);
    const size_t in_bytes_per_sample = self->num_channels * self->bytes_per_sample;
    size_t n_samples = MIN(out_size / out_bytes_per_sample, in_size / in_bytes_per_sample);
    // uint32_t rand = get_rand_32();
    for (size_t i = 0; i < n_samples; i++) {
        uint32_t sample;
        if (self->bytes_per_sample == 1) {
            sample = *in_buffer;
            sample <<= 8;
        } else if (self->bytes_per_sample == 2) {
            sample = in_buffer[0] | (in_buffer[1] << 8);
            sample ^= 0x8000;
        } else {
            sample = 0x8000;
        }
        in_buffer += in_bytes_per_sample;

        sample <<= self->pwm_bits;
        sample += self->error;
        // divmod_u32u32_rem(rand, self->divisor, &self->error);
        // sample += self->error;
        // rand = (rand << 1) | (rand >> 31);
        sample = divmod_u32u32_rem(sample, self->divisor, &self->error);
        *out_buffer++ = sample;
    }
    return n_samples;
}

static mp_uint_t audio_out_pwm_write_nonblock(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get(self_in);
    if (!audio_out_pwm_inited(self)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }

    const size_t in_bytes_per_sample = self->num_channels * self->bytes_per_sample;
    const size_t out_bytes_per_sample = sizeof(uint16_t);
    size_t ret = 0;
    size_t fragment_size = self->fragment[3];
    while (size - ret + fragment_size >= in_bytes_per_sample) {
        void *pwm_buf;
        size_t pwm_size = pico_fifo_get_buffer(&self->fifo, &pwm_buf);
        if (pwm_size < out_bytes_per_sample) {
            if (ret == 0) {
                *errcode = MP_EAGAIN;
                ret = MP_STREAM_ERROR;
            }
            break;
        }
        size_t n_samples;
        if (fragment_size) {
            memcpy(self->fragment + fragment_size, buf + ret, in_bytes_per_sample - fragment_size);
            n_samples = audio_out_pwm_transcode(self, pwm_buf, pwm_size, self->fragment, in_bytes_per_sample);
            assert(n_samples == 1);
            ret -= fragment_size;
            fragment_size = 0;
        } else {
            n_samples = audio_out_pwm_transcode(self, pwm_buf, pwm_size, buf + ret, size - ret);
        }

        pico_fifo_put_buffer(&self->fifo, n_samples * out_bytes_per_sample);
        ret += n_samples * in_bytes_per_sample;
    }
    if (size - ret + fragment_size < in_bytes_per_sample) {
        memcpy(self->fragment + fragment_size, buf + ret, size - ret);
        fragment_size += size - ret;
        ret = size;
    }
    self->fragment[3] = fragment_size;
    return ret;
}

static mp_uint_t audio_out_pwm_write_block(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get(self_in);
    return mp_poll_block(self_in, (void *)buf, size, errcode, audio_out_pwm_write_nonblock, MP_STREAM_POLL_WR, self->timeout, true);
}

static mp_obj_t audio_out_pwm_write(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
    size_t len = bufinfo.len;
    if ((n_args > 2) && (args[2] != mp_const_none)) {
        len = MIN(len, (size_t)mp_obj_get_int(args[2]));
    }
    int errcode;
    mp_uint_t ret = audio_out_pwm_write_block(args[0], bufinfo.buf, len, &errcode);
    return mp_stream_return(ret, errcode);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(audio_out_pwm_write_obj, 2, 3, audio_out_pwm_write);

static mp_uint_t audio_out_pwm_empty(mp_obj_t self_in, void *buf, mp_uint_t len, int *errcode) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get(self_in);
    if (!audio_out_pwm_inited(self)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }
    if (!pico_fifo_empty(&self->fifo)) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    return 0;
}

static mp_obj_t audio_out_pwm_drain(mp_obj_t self_in) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get_raise(self_in);
    pico_fifo_flush(&self->fifo);
    int errcode;
    mp_uint_t ret = mp_poll_block(self_in, NULL, 0, &errcode, audio_out_pwm_empty, MP_STREAM_POLL_WR, self->timeout, true);
    return mp_stream_return(ret, errcode);
}
static MP_DEFINE_CONST_FUN_OBJ_1(audio_out_pwm_drain_obj, audio_out_pwm_drain);

static mp_uint_t audio_out_pwm_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get(self_in);
    if (!audio_out_pwm_inited(self) && (request != MP_STREAM_CLOSE)) {
        *errcode = MP_EBADF;
        return MP_STREAM_ERROR;
    }
    mp_uint_t ret = 0;
    switch (request) {
        case MP_STREAM_FLUSH:
            pico_fifo_flush(&self->fifo);
            break;
        case MP_STREAM_TIMEOUT:
            ret = mp_stream_timeout(&self->timeout, arg, errcode);
            break;
        case MP_STREAM_CLOSE:
            ret = audio_out_pwm_close(self_in, errcode);
            break;
        case MP_STREAM_POLL_CTL:
            pico_fifo_acquire(&self->fifo);
            ret = mp_stream_poll_ctl(&self->poll, (void *)arg, errcode);
            pico_fifo_release(&self->fifo);
            break;
        default:
            *errcode = MP_EINVAL;
            ret = MP_STREAM_ERROR;
            break;
    }
    return ret;
}

static mp_obj_t audio_out_pwm_start(mp_obj_t self_in) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get_raise(self_in);
    pico_fifo_flush(&self->fifo);
    pico_fifo_set_enabled(&self->fifo, true);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(audio_out_pwm_start_obj, audio_out_pwm_start);

static mp_obj_t audio_out_pwm_stop(mp_obj_t self_in) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get_raise(self_in);
    pico_fifo_set_enabled(&self->fifo, false);
    pwm_set_both_levels(self->pwm_slice, self->top / 2, self->top / 2);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(audio_out_pwm_stop_obj, audio_out_pwm_stop);

#ifndef NDEBUG
#include <stdio.h>

static mp_obj_t audio_out_pwm_debug(mp_obj_t self_in) {
    audio_out_pwm_obj_t *self = audio_out_pwm_get(self_in);
    printf("audio_out_pwm %p\n", self);
    printf("  freq:        %lu\n", clock_get_hz(clk_sys));
    printf("  top:         %lu\n", self->top);
    printf("  divisor:     %lu\n", self->divisor);
    printf("  int_count:   %u\n", self->int_count);
    printf("  stalls:      %u\n", self->stalls);

    if (self->pwm_slice != -1u) {
        pico_pwm_debug(self->pwm_slice);
    }

    pico_fifo_debug(&self->fifo);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(audio_out_pwm_debug_obj, audio_out_pwm_debug);
#endif

static const mp_rom_map_elem_t audio_out_pwm_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&audio_out_pwm_del_obj) },

    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&audio_out_pwm_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout), MP_ROM_PTR(&mp_stream_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },

    { MP_ROM_QSTR(MP_QSTR_drain),  MP_ROM_PTR(&audio_out_pwm_drain_obj) },

    { MP_ROM_QSTR(MP_QSTR_start), MP_ROM_PTR(&audio_out_pwm_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop),  MP_ROM_PTR(&audio_out_pwm_stop_obj) },

    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_debug), MP_ROM_PTR(&audio_out_pwm_debug_obj) },
    #endif
};
static MP_DEFINE_CONST_DICT(audio_out_pwm_locals_dict, audio_out_pwm_locals_dict_table);

static const mp_stream_p_t audio_out_pwm_stream_p = {
    .write = audio_out_pwm_write_block,
    .ioctl = audio_out_pwm_ioctl,
    .is_text = 0,
    .can_poll = 1,
};

MP_DEFINE_CONST_OBJ_TYPE(
    audio_out_pwm_type,
    MP_QSTR_AudioOutPwm,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, audio_out_pwm_make_new,
    protocol, &audio_out_pwm_stream_p,
    locals_dict, &audio_out_pwm_locals_dict
    );
