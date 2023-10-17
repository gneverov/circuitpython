// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "pico/fifo.h"
#include "pico/pio.h"

#include "py/obj.h"
#include "py/stream_poll.h"


typedef struct {
    mp_obj_base_t base;
    PIO pio;
    pio_program_t program;
    uint loaded_offset;
    uint sm;
    pio_sm_config config;
    uint pin_mask;

    pico_fifo_t rx_fifo;
    pico_fifo_t tx_fifo;
    int rx_enabled;

    TickType_t timeout;
    mp_stream_poll_t poll;
    uint16_t instructions[32];

    uint stalls;
} state_machine_obj_t;

extern const mp_obj_type_t state_machine_type;
