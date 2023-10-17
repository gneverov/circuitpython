// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/dma.h"

typedef void (*pico_dma_handler_t)(uint channel, void *context);

void pico_dma_init(void);

void pico_dma_set_irq(uint channel, pico_dma_handler_t handler, void *context);

void pico_dma_clear_irq(uint channel);

void pico_dma_acknowledge_irq(uint channel);

void pico_dma_debug(uint channel);
