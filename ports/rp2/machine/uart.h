// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "pico/uart.h"

#include "py/stream_poll.h"


typedef struct {
    mp_obj_base_t base;
    int uart_num;
    pico_uart_t uart;
    TickType_t timeout;
    mp_stream_poll_t poll;
} uart_obj_t;

extern const mp_obj_type_t uart_type;
