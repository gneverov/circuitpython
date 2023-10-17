// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "pico/fifo.h"

#include "py/mphal.h"
#include "py/stream_poll.h"

typedef struct {
    mp_obj_base_t base;
    mp_hal_pin_obj_t a_pin;
    mp_hal_pin_obj_t b_pin;
    uint pwm_slice;
    pico_fifo_t fifo;
    uint32_t error;
    uint32_t top;
    uint32_t divisor;
    mp_stream_poll_t poll;
    TickType_t timeout;
    uint8_t fragment[4];

    uint num_channels;
    uint sample_rate;
    uint bytes_per_sample;
    uint pwm_bits;

    uint int_count;
    uint stalls;
} audio_out_pwm_obj_t;

extern const mp_obj_type_t audio_out_pwm_type;
