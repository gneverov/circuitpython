// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

enum {
    DEV_SDCARD0 = 0xb300,
    DEV_SDCARD1 = 0xb308,
};

void *sdcard_open(const char *fragment, int flags, mode_t mode, dev_t dev);
