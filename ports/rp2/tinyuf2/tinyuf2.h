// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/types.h>


void *tinyuf2_open(const char *fragment, int flags, mode_t mode, dev_t dev);
