// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "FreeRTOS.h"

#include "hardware/pio.h"

#define NUM_PIO_INTERRUPT_SOURCES 12


PIO pico_pio(uint pio_index);

typedef void (*pico_pio_handler_t)(PIO pio, enum pio_interrupt_source source, void *context, BaseType_t *pxHigherPriorityTaskWoken);

void pico_pio_init(void);

void pico_pio_set_irq(PIO pio, enum pio_interrupt_source source, pico_pio_handler_t handler, void *context);

void pico_pio_clear_irq(PIO pio, enum pio_interrupt_source source);

void pico_pio_debug(PIO pio);
