// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "newlib/event.h"

#include "pico/fifo.h"

#include "py/obj.h"


typedef struct {
    mp_obj_base_t base;
    int fd;
    struct event_file *event;
    uint a_pin;
    uint b_pin;
    uint pwm_slice;
    pico_fifo_t fifo;
    size_t threshold;
    uint32_t error;
    uint32_t top;
    uint32_t divisor;
    uint8_t fragment[4];

    uint num_channels;
    uint sample_rate;
    uint bytes_per_sample;
    uint pwm_bits;

    uint int_count;
    uint stalls;
} audio_out_pwm_obj_t;

extern const mp_obj_type_t audio_out_pwm_type;
