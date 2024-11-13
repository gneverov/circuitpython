// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "hardware/irq.h"


void pico_irq_set_enabled(uint num, bool enabled);

void pico_irq_remove_handler(uint num, irq_handler_t handler);
