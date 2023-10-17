#pragma once

// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "py/mphal.h"
#include "py/stream_poll.h"

typedef struct {
    mp_obj_base_t base;
    mp_hal_pin_obj_t pin;
    mp_stream_poll_t poll;
    TickType_t timeout;
    uint32_t events;
    uint32_t event_mask;
    int64_t pulse_down;
    int64_t pulse_up;
    int int_count;
} pin_obj_t;

extern const mp_obj_type_t pin_type;
