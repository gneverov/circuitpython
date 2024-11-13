// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "newlib/event.h"

#include "pico/fifo.h"
#include "pico/pio.h"

#include "py/obj.h"


typedef struct {
    mp_obj_base_t base;
    int fd;
    struct event_file *event;
    PIO pio;
    pio_program_t program;
    uint loaded_offset;
    uint sm;
    pio_sm_config config;
    uint pin_mask;

    pico_fifo_t rx_fifo;
    pico_fifo_t tx_fifo;
    size_t threshold;

    uint16_t instructions[32];

    uint stalls;
} state_machine_obj_t;

extern const mp_obj_type_t state_machine_type;
