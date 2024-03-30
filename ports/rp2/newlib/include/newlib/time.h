// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <time.h>

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
