/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Jeff Epler for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "common-hal/audiopwmio/PWMAudioOut.h"

#include "common-hal/rp2pio/Dma.h"
#include "peripherals/pwm.h"
#include "py/mperrno.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "src/rp2_common/hardware_clocks/include/hardware/clocks.h"
#include "src/rp2_common/hardware_dma/include/hardware/dma.h"
#include "src/rp2_common/hardware_gpio/include/hardware/gpio.h"
#include "src/rp2_common/hardware_pwm/include/hardware/pwm.h"

void common_hal_audiopwmio_pwmaudioout_init(audiopwmio_pwmaudioout_obj_t *self, const mp_obj_type_t *type) {
    self->base.type = type;
    self->a_channel = NULL;
    self->b_channel = NULL;
    self->pwm_slice = -1;
    self->dma_timer = -1;
    common_hal_rp2pio_dmaringbuf_init(&self->ringbuf, true);
}

static void _dmaringbuf_handler(rp2pio_dmaringbuf_t *ringbuf) {
    audiopwmio_pwmaudioout_obj_t *self = (audiopwmio_pwmaudioout_obj_t *)((char *)ringbuf - offsetof(audiopwmio_pwmaudioout_obj_t, ringbuf));
    self->int_count++;

    pwm_set_both_levels(self->pwm_slice, 0, (1u << self->output_bits) - 1);
    pwm_set_enabled(self->pwm_slice, false);
}

// Caller validates that pins are free.
void common_hal_audiopwmio_pwmaudioout_construct(audiopwmio_pwmaudioout_obj_t *self,
    const mcu_pin_obj_t *a_channel, const mcu_pin_obj_t *b_channel, uint ring_size_bits, uint max_transfer_count, uint channel_count, uint sample_rate, uint input_bytes, uint output_bits, bool phase_correct) {

    if (pwm_gpio_to_slice_num(a_channel->number) != pwm_gpio_to_slice_num(b_channel->number)) {
        mp_raise_ValueError(translate("Pins must share PWM slice"));
    }

    uint pwm_slice = pwm_gpio_to_slice_num(a_channel->number);
    if (!peripherals_pwm_claim(pwm_slice)) {
        const char *msg = "pwm";
        mp_obj_t msg_obj = mp_obj_new_str(msg, strlen(msg));
        mp_raise_OSError_errno_str(MP_EBUSY, msg_obj);
    }
    self->pwm_slice = pwm_slice;

    common_hal_mcu_pin_claim(a_channel);
    gpio_set_function(a_channel->number, GPIO_FUNC_PWM);
    gpio_set_drive_strength(a_channel->number, GPIO_DRIVE_STRENGTH_12MA);
    self->a_channel = a_channel;

    common_hal_mcu_pin_claim(b_channel);
    gpio_set_function(b_channel->number, GPIO_FUNC_PWM);
    gpio_set_drive_strength(b_channel->number, GPIO_DRIVE_STRENGTH_12MA);
    self->b_channel = b_channel;

    pwm_config c = pwm_get_default_config();
    pwm_config_set_output_polarity(&c, false, true);
    pwm_config_set_phase_correct(&c, phase_correct);
    pwm_config_set_wrap(&c, (1u << output_bits) - 2);
    pwm_init(self->pwm_slice, &c, false);

    if (!peripherals_dma_timer_claim(&self->dma_timer)) {
        const char *msg = "dma_timer";
        mp_obj_t msg_obj = mp_obj_new_str(msg, strlen(msg));
        mp_raise_OSError_errno_str(MP_EBUSY, msg_obj);
    }
    uint timer = self->dma_timer;
    dma_timer_set_fraction(timer, sample_rate >> 12, clock_get_hz(clk_sys) >> 12);

    uint dreq = dma_get_timer_dreq(self->dma_timer);
    if (!common_hal_rp2pio_dmaringbuf_alloc(&self->ringbuf, ring_size_bits, dreq, max_transfer_count, DMA_SIZE_16, false, &pwm_hw->slice[self->pwm_slice].cc)) {
        const char *msg = "dma_channel";
        mp_obj_t msg_obj = mp_obj_new_str(msg, strlen(msg));
        mp_raise_OSError_errno_str(errno, msg_obj);
    }

    pwm_set_both_levels(self->pwm_slice, 0, (1u << output_bits) - 1);
    pwm_set_enabled(self->pwm_slice, true);
    pwm_set_enabled(self->pwm_slice, false);

    self->channel_count = channel_count;
    self->input_bytes = input_bytes;
    self->output_bits = output_bits;

    common_hal_rp2pio_dmaringbuf_set_handler(&self->ringbuf, _dmaringbuf_handler);
}

bool common_hal_audiopwmio_pwmaudioout_deinited(audiopwmio_pwmaudioout_obj_t *self) {
    return self->pwm_slice == -1u;
}

void common_hal_audiopwmio_pwmaudioout_deinit(audiopwmio_pwmaudioout_obj_t *self) {
    common_hal_rp2pio_dmaringbuf_deinit(&self->ringbuf);

    if (self->dma_timer != -1u) {
        peripherals_dma_timer_unclaim(self->dma_timer);
        self->dma_timer = -1;
    }

    if (self->pwm_slice != -1u) {
        peripherals_pwm_unclaim(self->pwm_slice);
        self->pwm_slice = -1;
    }

    if (self->a_channel != NULL) {
        common_hal_reset_pin(self->a_channel);
        self->a_channel = NULL;
    }

    if (self->b_channel != NULL) {
        common_hal_reset_pin(self->b_channel);
        self->b_channel = NULL;
    }
}

mp_uint_t common_hal_audiopwmio_pwmaudioout_write(mp_obj_t self_obj, const void *buf, mp_uint_t size, int *errcode) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    if (size < sizeof(self->input_bytes)) {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }

    void *pwm_buf;
    size_t pwm_size = common_hal_rp2pio_dmaringbuf_acquire(&self->ringbuf, &pwm_buf) >> DMA_SIZE_16;
    if (pwm_size == 0) {
        *errcode = MP_EAGAIN;
        return MP_STREAM_ERROR;
    }
    size_t n = MIN(size / (self->channel_count * self->input_bytes), pwm_size);
    if (self->input_bytes == 1) {
        for (size_t i = 0; i < n; i++) {
            uint16_t sample = ((uint8_t *)buf)[i * self->channel_count];
            sample >>= 8 - self->output_bits;
            ((uint16_t *)pwm_buf)[i] = sample;
        }
    } else if (self->input_bytes == 2) {
        for (size_t i = 0; i < n; i++) {
            uint16_t sample = ((int16_t *)buf)[i * self->channel_count];
            sample ^= 0x8000;
            sample >>= 16 - self->output_bits;
            ((uint16_t *)pwm_buf)[i] = sample;
        }
    } else {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }
    common_hal_rp2pio_dmaringbuf_release(&self->ringbuf, n << DMA_SIZE_16);
    return n * self->channel_count * self->input_bytes;
}

mp_uint_t common_hal_audiopwmio_pwmaudioout_ioctl(mp_obj_t self_obj, mp_uint_t request, uintptr_t arg, int *errcode) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_obj);
    switch (request) {
        case MP_STREAM_FLUSH:
            common_hal_rp2pio_dmaringbuf_flush(&self->ringbuf);
            return 0;
        case MP_STREAM_CLOSE:
            common_hal_audiopwmio_pwmaudioout_deinit(self);
            return 0;
        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

size_t common_hal_audiopwmio_pwmaudioout_play(audiopwmio_pwmaudioout_obj_t *self, const void *buf, size_t len) {
    int errcode;
    mp_uint_t result = mp_stream_write_exactly(MP_OBJ_FROM_PTR(self), buf, len, &errcode);
    if (result == MP_STREAM_ERROR) {
        mp_raise_OSError(errcode);
    }

    common_hal_rp2pio_dmaringbuf_flush(&self->ringbuf);
    pwm_set_enabled(self->pwm_slice, true);
    return result;
}

void common_hal_audiopwmio_pwmaudioout_stop(audiopwmio_pwmaudioout_obj_t *self) {
    common_hal_rp2pio_dmaringbuf_clear(&self->ringbuf);
    pwm_set_both_levels(self->pwm_slice, 0, (1u << self->output_bits) - 1);
    pwm_set_enabled(self->pwm_slice, false);
}

bool common_hal_audiopwmio_pwmaudioout_get_playing(audiopwmio_pwmaudioout_obj_t *self) {
    return self->ringbuf.trans_count != 0;
}

uint common_hal_audiopwmio_pwmaudioout_get_stalled(audiopwmio_pwmaudioout_obj_t *self) {
    uint stalled = self->int_count;
    self->int_count = 0;
    return stalled;
}

uint common_hal_audiopwmio_pwmaudioout_get_available(audiopwmio_pwmaudioout_obj_t *self) {
    rp2pio_dmaringbuf_t *ringbuf = &self->ringbuf;
    return ringbuf->size - (ringbuf->next_write - ringbuf->next_read);
}

#ifndef NDEBUG
void common_hal_audiopwmio_pwmaudioout_debug(const mp_print_t *print, audiopwmio_pwmaudioout_obj_t *self) {
    mp_printf(print, "pwmaudioout %p\n", self);
    if (self->dma_timer != -1u) {
        mp_printf(print, "  dma_timer:   %d\n", self->dma_timer);
    }
    mp_printf(print, "  input_bytes: %d\n", self->input_bytes);
    mp_printf(print, "  output_bits: %d\n", self->output_bits);
    mp_printf(print, "  int_count:   %d\n", self->int_count);

    if (self->pwm_slice != -1u) {
        peripherals_pwm_debug(print, self->pwm_slice);
    }

    common_hal_rp2pio_dmaringbuf_debug(print, &self->ringbuf);
}
#endif
