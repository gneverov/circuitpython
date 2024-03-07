// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

int tud_msc_insert(uint8_t lun, const char *device, int flags);

int tud_msc_eject(uint8_t lun);

int tud_msc_ready(uint8_t lun);
