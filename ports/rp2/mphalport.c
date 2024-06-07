/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <errno.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "py/runtime.h"
#include "shared/timeutils/timeutils.h"
#include "hardware/rtc.h"
#include "pico/time.h"
#include "pico/unique_id.h"

// This needs to be added to the result of time_us_64() to get the number of
// microseconds since the Epoch.
static uint64_t time_us_64_offset_from_epoch;

uintptr_t mp_hal_stdio_poll(uintptr_t poll_flags) {
    panic("mp_hal_stdio_poll not implemented");
}

// Receive single character
int mp_hal_stdin_rx_chr(void) {
    int ch;
    do {
        MP_THREAD_GIL_EXIT();
        ch = getchar();
        MP_THREAD_GIL_ENTER();
        mp_handle_pending(false);
    }
    while ((ch == -1) && (errno == EINTR));

    return ch;
}

// Send string of given length
mp_uint_t mp_hal_stdout_tx_strn(const char *str, mp_uint_t len) {
    size_t size = fwrite(str, sizeof(char), len, stdout);
    fflush(stdout);
    return size;
}

void mp_hal_delay_ms(mp_uint_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void mp_hal_time_ns_set_from_rtc(void) {
    // Delay at least one RTC clock cycle so it's registers have updated with the most
    // recent time settings.
    sleep_us(23);

    // Sample RTC and time_us_64() as close together as possible, so the offset
    // calculated for the latter can be as accurate as possible.
    datetime_t t;
    rtc_get_datetime(&t);
    uint64_t us = time_us_64();

    // Calculate the difference between the RTC Epoch seconds and time_us_64().
    uint64_t s = timeutils_seconds_since_epoch(t.year, t.month, t.day, t.hour, t.min, t.sec);
    time_us_64_offset_from_epoch = (uint64_t)s * 1000000ULL - us;
}

uint64_t mp_hal_time_ns(void) {
    // The RTC only has seconds resolution, so instead use time_us_64() to get a more
    // precise measure of Epoch time.  Both these "clocks" are clocked from the same
    // source so they remain synchronised, and only differ by a fixed offset (calculated
    // in mp_hal_time_ns_set_from_rtc).
    return (time_us_64_offset_from_epoch + time_us_64()) * 1000ULL;
}

// Generate a random locally administered MAC address (LAA)
void mp_hal_generate_laa_mac(int idx, uint8_t buf[6]) {
    #ifndef NDEBUG
    printf("Warning: No MAC in OTP, generating MAC from board id\n");
    #endif
    pico_unique_board_id_t pid;
    pico_get_unique_board_id(&pid);
    buf[0] = 0x02; // LAA range
    buf[1] = (pid.id[7] << 4) | (pid.id[6] & 0xf);
    buf[2] = (pid.id[5] << 4) | (pid.id[4] & 0xf);
    buf[3] = (pid.id[3] << 4) | (pid.id[2] & 0xf);
    buf[4] = pid.id[1];
    buf[5] = (pid.id[0] << 2) | idx;
}

// A board can override this if needed
MP_WEAK void mp_hal_get_mac(int idx, uint8_t buf[6]) {
    #if MICROPY_PY_NETWORK_CYW43
    // The mac should come from cyw43 otp when CYW43_USE_OTP_MAC is defined
    // This is loaded into the state after the driver is initialised
    // cyw43_hal_generate_laa_mac is only called by the driver to generate a mac if otp is not set
    if (idx == MP_HAL_MAC_WLAN0) {
        memcpy(buf, cyw43_state.mac, 6);
        return;
    }
    #endif
    mp_hal_generate_laa_mac(idx, buf);
}

void mp_hal_get_mac_ascii(int idx, size_t chr_off, size_t chr_len, char *dest) {
    static const char hexchr[16] = "0123456789ABCDEF";
    uint8_t mac[6];
    mp_hal_get_mac(idx, mac);
    for (; chr_len; ++chr_off, --chr_len) {
        *dest++ = hexchr[mac[chr_off >> 1] >> (4 * (1 - (chr_off & 1))) & 0xf];
    }
}
