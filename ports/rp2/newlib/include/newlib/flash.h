// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <sys/types.h>


enum {
    DEV_FLASH = 0x1f10,
};

void *flash_open(const char *fragment, int flags, mode_t mode, dev_t dev);

void flash_lockout_start(void);

void flash_lockout_end(void);
