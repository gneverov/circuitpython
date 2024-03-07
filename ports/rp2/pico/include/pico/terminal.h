// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once


enum {
    DEV_TTYS0 = 0x0440,
    DEV_TTYS1 = 0x0441,
};

void *terminal_open(const char *fragment, int flags, mode_t mode, dev_t dev);
