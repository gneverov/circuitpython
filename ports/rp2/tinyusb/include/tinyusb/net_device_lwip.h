// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>

#include "tusb.h"


void tud_network_init(void);

void tud_network_set_link(bool up);
