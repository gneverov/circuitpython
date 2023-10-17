// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "tusb.h"


typedef enum {
    TUD_CDC_RX,
    TUD_CDC_RX_WANTED,
    TUD_CDC_TX_COMPLETE,
    TUD_CDC_LINE_STATE,
    TUD_CDC_LINE_CODING,
    TUD_CDC_SEND_BREAK,
} tud_cdc_cb_type_t;

typedef union {
    struct {
        char wanted_char;
    } rx_wanted;
    struct {
        bool dtr;
        bool rts;
    } line_state;
    struct {
        cdc_line_coding_t const *p_line_coding;
    } line_coding;
    struct {
        uint16_t duration_ms;
    } send_break;
} tud_cdc_cb_args_t;


typedef void (*tud_cdc_cb_t)(void *context, tud_cdc_cb_type_t cb_type, tud_cdc_cb_args_t *cb_args);

bool tud_cdc_set_cb(uint8_t itf, tud_cdc_cb_t cb, void *context);

void tud_cdc_clear_cb(uint8_t itf);
