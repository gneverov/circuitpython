// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/gpio.h"

typedef void (*pico_gpio_handler_t)(uint gpio, uint32_t event_mask, void *context);

void pico_gpio_set_irq_enabled(uint gpio, uint32_t events, bool enabled);

bool pico_gpio_add_handler(uint gpio, pico_gpio_handler_t handler, void *context);

bool pico_gpio_remove_handler(uint gpio);

void pico_gpio_debug(uint gpio);
