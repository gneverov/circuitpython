// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/types.h>


enum {
    DEV_TTYUSB0 = 0xbc00,
    DEV_TTYUSB1 = 0xbc01,
    DEV_TTYUSB2 = 0xbc02,
    DEV_TTYUSB3 = 0xbc03,
};

void *terminal_usb_open(const char *fragment, int flags, mode_t mode, dev_t dev);
