// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/dma.h"

typedef void (*pico_gpio_handler_t)(uint gpio, uint32_t event_mask, void *context);

void pico_gpio_init(void);

void pico_gpio_set_irq(uint gpio, pico_gpio_handler_t handler, void *context);

void pico_gpio_clear_irq(uint gpio);

void pico_gpio_debug(uint gpio);
