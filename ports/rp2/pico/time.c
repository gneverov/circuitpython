// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <sys/time.h>

#include "pico/aon_timer.h"
#include "pico/time.h"


int __real_settimeofday(const struct timeval *tv, const struct timezone *tz);

int __wrap_settimeofday(const struct timeval *tv, const struct timezone *tz) {
    int ret = __real_settimeofday(tv, tz);
    if (ret >= 0) {
        struct timespec ts;
        TIMEVAL_TO_TIMESPEC(tv, &ts);
        if (aon_timer_is_running()) {
            aon_timer_set_time(&ts);
        } else {
            aon_timer_start(&ts);
        }
    }
    return ret;
}
