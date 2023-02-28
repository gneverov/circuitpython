/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Scott Shawcroft for Adafruit Industries
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

#include <stdint.h>

#include "shared-bindings/audiopwmio/PWMAudioOut.h"
#include "common-hal/audiopwmio/PWMAudioOut.h"
#include "py/objproperty.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "shared/runtime/context_manager_helpers.h"
#include "shared-bindings/microcontroller/Pin.h"
#include "shared-bindings/util.h"
#include "supervisor/shared/translate/translate.h"

//| class PWMAudioOut:
//|     """Output an analog audio signal by varying the PWM duty cycle."""
//|
//|     def __init__(
//|         self,
//|         left_channel: microcontroller.Pin,
//|         *,
//|         right_channel: Optional[microcontroller.Pin] = None,
//|         quiescent_value: int = 0x8000
//|     ) -> None:
//|         """Create a PWMAudioOut object associated with the given pin(s). This allows you to
//|         play audio signals out on the given pin(s).  In contrast to mod:`audioio`,
//|         the pin(s) specified are digital pins, and are driven with a device-dependent PWM
//|         signal.
//|
//|         :param ~microcontroller.Pin left_channel: The pin to output the left channel to
//|         :param ~microcontroller.Pin right_channel: The pin to output the right channel to
//|         :param int quiescent_value: The output value when no signal is present. Samples should start
//|             and end with this value to prevent audible popping.
//|
//|         Simple 8ksps 440 Hz sin wave::
//|
//|           import audiocore
//|           import audiopwmio
//|           import board
//|           import array
//|           import time
//|           import math
//|
//|           # Generate one period of sine wav.
//|           length = 8000 // 440
//|           sine_wave = array.array("H", [0] * length)
//|           for i in range(length):
//|               sine_wave[i] = int(math.sin(math.pi * 2 * i / length) * (2 ** 15) + 2 ** 15)
//|
//|           dac = audiopwmio.PWMAudioOut(board.SPEAKER)
//|           sine_wave = audiocore.RawSample(sine_wave, sample_rate=8000)
//|           dac.play(sine_wave, loop=True)
//|           time.sleep(1)
//|           dac.stop()
//|
//|         Playing a wave file from flash::
//|
//|           import board
//|           import audiocore
//|           import audiopwmio
//|           import digitalio
//|
//|           # Required for CircuitPlayground Express
//|           speaker_enable = digitalio.DigitalInOut(board.SPEAKER_ENABLE)
//|           speaker_enable.switch_to_output(value=True)
//|
//|           data = open("cplay-5.1-16bit-16khz.wav", "rb")
//|           wav = audiocore.WaveFile(data)
//|           a = audiopwmio.PWMAudioOut(board.SPEAKER)
//|
//|           print("playing")
//|           a.play(wav)
//|           while a.playing:
//|             pass
//|           print("stopped")"""
//|         ...
STATIC mp_obj_t audiopwmio_pwmaudioout_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    enum { ARG_a_channel, ARG_b_channel, ARG_ring_size_bits, ARG_max_transfer_count, ARG_channel_count, ARG_sample_rate, ARG_input_bytes, ARG_output_bits, ARG_phase_correct };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_a_channel, MP_ARG_OBJ | MP_ARG_REQUIRED },
        { MP_QSTR_b_channel, MP_ARG_OBJ | MP_ARG_REQUIRED },
        { MP_QSTR_ring_size_bits, MP_ARG_INT, {.u_int = 9} },
        { MP_QSTR_max_transfer_count, MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_channel_count, MP_ARG_INT | MP_ARG_REQUIRED },
        { MP_QSTR_sample_rate, MP_ARG_INT | MP_ARG_REQUIRED },
        { MP_QSTR_input_bytes, MP_ARG_INT | MP_ARG_REQUIRED },
        { MP_QSTR_output_bits, MP_ARG_INT | MP_ARG_REQUIRED },
        { MP_QSTR_phase_correct, MP_ARG_BOOL, {.u_bool = true}},
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    const mcu_pin_obj_t *a_channel_pin =
        validate_obj_is_free_pin(args[ARG_a_channel].u_obj, MP_QSTR_a_channel);
    const mcu_pin_obj_t *b_channel_pin =
        validate_obj_is_free_pin_or_none(args[ARG_b_channel].u_obj, MP_QSTR_b_channel);

    // create AudioOut object from the given pin
    audiopwmio_pwmaudioout_obj_t *self = m_new_obj(audiopwmio_pwmaudioout_obj_t);
    common_hal_audiopwmio_pwmaudioout_init(self, &audiopwmio_pwmaudioout_type);
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        common_hal_audiopwmio_pwmaudioout_construct(self, a_channel_pin, b_channel_pin, args[ARG_ring_size_bits].u_int, args[ARG_max_transfer_count].u_int, args[ARG_channel_count].u_int, args[ARG_sample_rate].u_int, args[ARG_input_bytes].u_int, args[ARG_output_bits].u_int, args[ARG_phase_correct].u_bool);
        nlr_pop();
    } else {
        common_hal_audiopwmio_pwmaudioout_deinit(self);
        nlr_jump(nlr.ret_val);
    }

    return MP_OBJ_FROM_PTR(self);
}

//|     def deinit(self) -> None:
//|         """Deinitialises the PWMAudioOut and releases any hardware resources for reuse."""
//|         ...
STATIC mp_obj_t audiopwmio_pwmaudioout_deinit(mp_obj_t self_in) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_in);
    common_hal_audiopwmio_pwmaudioout_deinit(self);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(audiopwmio_pwmaudioout_deinit_obj, audiopwmio_pwmaudioout_deinit);

STATIC void check_for_deinit(audiopwmio_pwmaudioout_obj_t *self) {
    if (common_hal_audiopwmio_pwmaudioout_deinited(self)) {
        raise_deinited_error();
    }
}
//|     def __enter__(self) -> PWMAudioOut:
//|         """No-op used by Context Managers."""
//|         ...
//  Provided by context manager helper.

//|     def __exit__(self) -> None:
//|         """Automatically deinitializes the hardware when exiting a context. See
//|         :ref:`lifetime-and-contextmanagers` for more info."""
//|         ...
STATIC mp_obj_t audiopwmio_pwmaudioout_obj___exit__(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    common_hal_audiopwmio_pwmaudioout_deinit(args[0]);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(audiopwmio_pwmaudioout___exit___obj, 4, 4, audiopwmio_pwmaudioout_obj___exit__);


//|     def play(self, sample: circuitpython_typing.AudioSample, *, loop: bool = False) -> None:
//|         """Plays the sample once when loop=False and continuously when loop=True.
//|         Does not block. Use `playing` to block.
//|
//|         Sample must be an `audiocore.WaveFile`, `audiocore.RawSample`, `audiomixer.Mixer` or `audiomp3.MP3Decoder`.
//|
//|         The sample itself should consist of 16 bit samples. Microcontrollers with a lower output
//|         resolution will use the highest order bits to output."""
//|         ...
STATIC mp_obj_t audiopwmio_pwmaudioout_obj_play(mp_obj_t self_in, mp_obj_t buffer_obj) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_buffer_info_t buffer;
    mp_get_buffer_raise(buffer_obj, &buffer, MP_BUFFER_READ);

    return mp_obj_new_int(common_hal_audiopwmio_pwmaudioout_play(self, buffer.buf, buffer.len));
}
MP_DEFINE_CONST_FUN_OBJ_2(audiopwmio_pwmaudioout_play_obj, audiopwmio_pwmaudioout_obj_play);

//|     def stop(self) -> None:
//|         """Stops playback and resets to the start of the sample."""
//|         ...
STATIC mp_obj_t audiopwmio_pwmaudioout_obj_stop(mp_obj_t self_in) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_in);
    check_for_deinit(self);
    common_hal_audiopwmio_pwmaudioout_stop(self);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(audiopwmio_pwmaudioout_stop_obj, audiopwmio_pwmaudioout_obj_stop);

#ifndef NDEBUG
STATIC mp_obj_t audiopwmio_pwmaudioout_debug(mp_obj_t self_in) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_in);
    common_hal_audiopwmio_pwmaudioout_debug(MICROPY_DEBUG_PRINTER, self);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(audiopwmio_pwmaudioout_debug_obj, audiopwmio_pwmaudioout_debug);
#endif

//|     playing: bool
//|     """True when an audio sample is being output even if `paused`. (read-only)"""
STATIC mp_obj_t audiopwmio_pwmaudioout_obj_get_playing(mp_obj_t self_in) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_in);
    check_for_deinit(self);
    return mp_obj_new_bool(common_hal_audiopwmio_pwmaudioout_get_playing(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(audiopwmio_pwmaudioout_get_playing_obj, audiopwmio_pwmaudioout_obj_get_playing);

MP_PROPERTY_GETTER(audiopwmio_pwmaudioout_playing_obj,
    (mp_obj_t)&audiopwmio_pwmaudioout_get_playing_obj);

//|     stalled: int
//|     """True when an audio sample is being output even if `paused`. (read-only)"""
STATIC mp_obj_t audiopwmio_pwmaudioout_obj_get_stalled(mp_obj_t self_in) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_in);
    check_for_deinit(self);
    return mp_obj_new_int(common_hal_audiopwmio_pwmaudioout_get_stalled(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(audiopwmio_pwmaudioout_get_stalled_obj, audiopwmio_pwmaudioout_obj_get_stalled);

MP_PROPERTY_GETTER(audiopwmio_pwmaudioout_stalled_obj, (mp_obj_t)&audiopwmio_pwmaudioout_get_stalled_obj);

//|     stalled: int
//|     """True when an audio sample is being output even if `paused`. (read-only)"""
//|
STATIC mp_obj_t audiopwmio_pwmaudioout_obj_get_available(mp_obj_t self_in) {
    audiopwmio_pwmaudioout_obj_t *self = MP_OBJ_TO_PTR(self_in);
    check_for_deinit(self);
    return mp_obj_new_int(common_hal_audiopwmio_pwmaudioout_get_available(self));
}
MP_DEFINE_CONST_FUN_OBJ_1(audiopwmio_pwmaudioout_get_available_obj, audiopwmio_pwmaudioout_obj_get_available);

MP_PROPERTY_GETTER(audiopwmio_pwmaudioout_available_obj, (mp_obj_t)&audiopwmio_pwmaudioout_get_available_obj);

STATIC const mp_rom_map_elem_t audiopwmio_pwmaudioout_locals_dict_table[] = {
    // Methods
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&audiopwmio_pwmaudioout_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&default___enter___obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&audiopwmio_pwmaudioout___exit___obj) },
    { MP_ROM_QSTR(MP_QSTR_play), MP_ROM_PTR(&audiopwmio_pwmaudioout_play_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&audiopwmio_pwmaudioout_stop_obj) },

    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },

    // Properties
    { MP_ROM_QSTR(MP_QSTR_playing), MP_ROM_PTR(&audiopwmio_pwmaudioout_playing_obj) },
    { MP_ROM_QSTR(MP_QSTR_stalled), MP_ROM_PTR(&audiopwmio_pwmaudioout_stalled_obj) },
    { MP_ROM_QSTR(MP_QSTR_available), MP_ROM_PTR(&audiopwmio_pwmaudioout_available_obj) },

    #ifndef NDEBUG
    { MP_ROM_QSTR(MP_QSTR_debug), MP_ROM_PTR(&audiopwmio_pwmaudioout_debug_obj) },
    #endif
};
STATIC MP_DEFINE_CONST_DICT(audiopwmio_pwmaudioout_locals_dict, audiopwmio_pwmaudioout_locals_dict_table);

STATIC const mp_stream_p_t audiopwmio_pwmaudioout_proto = {
    MP_PROTO_IMPLEMENT(MP_QSTR_protocol_stream)
    .write = common_hal_audiopwmio_pwmaudioout_write,
    .ioctl = common_hal_audiopwmio_pwmaudioout_ioctl,
};

const mp_obj_type_t audiopwmio_pwmaudioout_type = {
    { &mp_type_type },
    .name = MP_QSTR_PWMAudioOut,
    .flags = MP_TYPE_FLAG_EXTENDED,
    .make_new = audiopwmio_pwmaudioout_make_new,
    .locals_dict = (mp_obj_dict_t *)&audiopwmio_pwmaudioout_locals_dict,
    MP_TYPE_EXTENDED_FIELDS(
        .protocol = &audiopwmio_pwmaudioout_proto,
        ),
};
