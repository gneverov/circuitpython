// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <sys/types.h>


enum {
    DEV_DHARA = 0x3c00,
};

void *dhara_open(const char *fragment, int flags, mode_t mode, dev_t dev);
