// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "mtd.h"


void mtd_flash_probe(struct mtd_device *device);
void mtd_psram_probe(struct mtd_device *device);
