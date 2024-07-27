/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2021 Damien P. George
 * Copyright (c) 2023 Gregory Neverov
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

#include <fcntl.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/unistd.h>

#include "FreeRTOS.h"
#include "freertos/interrupts.h"
#include "task.h"

#if MICROPY_PY_LWIP
#include "lwip/lwip_init.h"
#endif
#include "newlib/flash_lockout.h"
#include "newlib/mount.h"
#include "newlib/newlib.h"
#include "newlib/thread.h"
#include "pico/dma.h"
#include "pico/gpio.h"
#include "pico/pio.h"
#include "pico/terminal.h"
#include "tinyusb/msc_device.h"
#include "tinyusb/net_device_lwip.h"
#include "tinyusb/terminal.h"
#include "tinyusb/tusb_config.h"
#include "tinyusb/tusb_lock.h"

#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/gc_handle.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/stackctrl.h"
#include "extmod/modbluetooth.h"
#include "extmod/modnetwork.h"
#include "extmod/freeze/freeze.h"
#include "extmod/modsignal.h"
#include "shared/readline/readline.h"
#include "shared/runtime/gchelper.h"
#include "shared/runtime/pyexec.h"
#include "tusb.h"
#include "modmachine.h"
#include "modrp2.h"
#include "mpbthciport.h"
#include "genhdr/mpversion.h"

#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "pico/unique_id.h"
#include "hardware/rtc.h"
#include "hardware/structs/rosc.h"


// Embed version info in the binary in machine readable form
bi_decl(bi_program_version_string(MICROPY_GIT_TAG));

// Add a section to the picotool output similar to program features, but for frozen modules
// (it will aggregate BINARY_INFO_ID_MP_FROZEN binary info)
bi_decl(bi_program_feature_group_with_flags(BINARY_INFO_TAG_MICROPYTHON,
    BINARY_INFO_ID_MP_FROZEN, "frozen modules",
    BI_NAMED_GROUP_SEPARATE_COMMAS | BI_NAMED_GROUP_SORT_ALPHA));

void mp_main(uint8_t *stack_bottom, uint8_t *stack_top, uint8_t *gc_heap_start, uint8_t *gc_heap_end) {
    #if MICROPY_HW_ENABLE_UART_REPL
    bi_decl(bi_program_feature("UART REPL"))
    #endif

    #if MICROPY_HW_USB_CDC
    bi_decl(bi_program_feature("USB REPL"))
    #endif

    #if MICROPY_PY_THREAD
    bi_decl(bi_program_feature("thread support"))
    mp_thread_init();
    mp_thread_set_state(&mp_state_ctx.thread);
    #endif

    // Start and initialise the RTC
    datetime_t t = {
        .year = 2021,
        .month = 1,
        .day = 1,
        .dotw = 4, // 0 is Monday, so 4 is Friday
        .hour = 0,
        .min = 0,
        .sec = 0,
    };
    rtc_init();
    rtc_set_datetime(&t);
    mp_hal_time_ns_set_from_rtc();

    // Initialise stack extents and GC heap.
    mp_stack_set_top(stack_top);
    mp_stack_set_limit(stack_top - stack_bottom - 256);

    for (;;) {
        gc_init(gc_heap_start, gc_heap_end);

        // Initialise MicroPython runtime.
        mp_init();
        mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
        freeze_init();

        // Initialise sub-systems.
        readline_init0();

        #if MICROPY_PY_BLUETOOTH
        mp_bluetooth_hci_init();
        #endif

        signal_init();

        // Execute user scripts.
        int ret = pyexec_file_if_exists("boot.py");
        if (ret & PYEXEC_FORCED_EXIT) {
            goto soft_reset_exit;
        }
        if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL && ret != 0) {
            ret = pyexec_file_if_exists("main.py");
            if (ret & PYEXEC_FORCED_EXIT) {
                goto soft_reset_exit;
            }
        }

        for (;;) {
            if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
                if (pyexec_raw_repl() != 0) {
                    break;
                }
            } else {
                if (pyexec_friendly_repl() != 0) {
                    break;
                }
            }
        }

    soft_reset_exit:
        mp_printf(&mp_plat_print, "MPY: soft reboot\n");
        signal_deinit();
        #if MICROPY_PY_BLUETOOTH
        mp_bluetooth_deinit();
        #endif
        machine_pwm_deinit_all();
        #if MICROPY_PY_THREAD
        mp_thread_deinit();
        #endif
        gc_sweep_all();
        gc_handle_collect(true);
        mp_deinit();
    }
}

thread_t *mp_thread;
StaticTask_t mp_taskdef;

#define DEFAULT_TTY "/dev/ttyUSB0"
#define DEFAULT_GC_HEAP (96 << 10)
#define MIN_GC_HEAP (8 << 10)
#define DEFAULT_MP_STACK (8 << 10)
#define MIN_MP_STACK (4 << 10)

void mp_task(void *params) {
    flash_lockout_init();

    size_t mp_stack_size = (uintptr_t)params;
    const char *gc_heap_str = getenv("GC_HEAP");
    size_t gc_heap_size = gc_heap_str ? atoi(gc_heap_str) : DEFAULT_GC_HEAP;
    gc_heap_size = MAX(gc_heap_size, MIN_GC_HEAP);
    uint8_t *gc_heap = malloc(gc_heap_size);
    while (!gc_heap) {
        gc_heap_size /= 2;
        gc_heap = malloc(gc_heap_size);
    }

    #if CFG_TUD_ENABLED
    while (!tud_inited()) {
        portYIELD();
    }
    #endif

    const char *tty = getenv("TTY");
    if (!tty) {
        tty = DEFAULT_TTY;
    }
    if (tty) {
        int fd = open(tty, O_RDWR, 0);
        if (fd >= 0) {
            // Success open of tty installs it on stdio fds, so we an close our fd.
            close(fd);
        } else {
            perror("failed up open terminal");
        }
    }

    TaskStatus_t task_status;
    vTaskGetInfo(NULL, &task_status, pdFALSE, eRunning);
    uint8_t *mp_stack = (uint8_t *)task_status.pxStackBase;
    mp_main(mp_stack, mp_stack + mp_stack_size, gc_heap, gc_heap + gc_heap_size);

    free(gc_heap);
}

void mp_task_interrupt(void) {
    thread_interrupt(mp_thread);
}

#if CFG_TUD_ENABLED
void mp_tud_task(void *params) {
    UBaseType_t save = set_interrupt_core_affinity();
    tud_init(TUD_OPT_RHPORT);
    clear_interrupt_core_affinity(save);
    tud_disconnect();

    #if MICROPY_PY_LWIP && (CFG_TUD_ECM_RNDIS || CFG_TUD_NCM)
    lwip_wait();
    tud_network_init();
    #endif

    tud_connect();

    while (1) {
        tud_task();
    }
    vTaskDelete(NULL);
}
#endif

#if CFG_TUH_ENABLED
void mp_tuh_task(void *params) {
    UBaseType_t save = set_interrupt_core_affinity();
    tuh_init(TUH_OPT_RHPORT);
    clear_interrupt_core_affinity(save);

    while (1) {
        tuh_task();
    }
    vTaskDelete(NULL);
}
#endif

static void set_default_time(void) {
    tzset();
    struct tm tm = {
        .tm_year = 124,
        .tm_mon = 0,
        .tm_mday = 1,
    };
    struct timeval tv = {
        mktime(&tm),
        0,
    };
    settimeofday(&tv, NULL);
}

static int mount_root_fs(void) {
    char *root = getenv("ROOT");
    if (!root) {
        return 0;
    }
    char device[64], filesystemtype[16];
    unsigned long flags = 0;
    if (sscanf(root, "%s %s %lu", device, filesystemtype, &flags) < 2) {
        errno = EINVAL;
        goto error;
    }
    if (mount(device, "/", filesystemtype, flags, NULL) >= 0) {
        return 0;
    }
    if ((flags & MS_RDONLY) || (errno != ENODEV)) {
        goto error;
    }
    // Mount failed due to no filesystem. Try mkfs.
    if (mkfs(device, filesystemtype, NULL) < 0) {
        goto error;
    }
    if (mount(device, "/", filesystemtype, flags, NULL) >= 0) {
        return 0;
    }

error:
    perror("failed to mount root filesystem");
    return -1;
}

int main(int argc, char **argv) {
    env_init();
    set_default_time();

    mount(NULL, "/dev", "devfs", 0, NULL);

    mount_root_fs();

    #if MICROPY_PY_LWIP
    lwip_helper_init();
    #endif

    #if CFG_TUD_ENABLED
    xTaskCreate(mp_tud_task, "tud", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    #endif

    #if CFG_TUH_ENABLED
    xTaskCreate(mp_tuh_task, "tuh", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    #endif

    const char *mp_stack_str = getenv("MP_STACK");
    size_t mp_stack_size = mp_stack_str ? atoi(mp_stack_str) : DEFAULT_MP_STACK;
    mp_stack_size = MAX(mp_stack_size, MIN_MP_STACK);
    mp_thread = thread_create(mp_task, "mp", mp_stack_size / sizeof(StackType_t), (void *)mp_stack_size, 1);
    vTaskStartScheduler();
    return 0;
}

void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    #if MICROPY_PY_THREAD
    mp_thread_gc_others();
    #endif
    #if MICROPY_FREERTOS
    gc_handle_collect(false);
    #endif
    freeze_gc();
    gc_collect_end();
}

void nlr_jump_fail(void *val) {
    mp_printf(&mp_plat_print, "FATAL: uncaught exception %p\n", val);
    mp_obj_print_exception(&mp_plat_print, MP_OBJ_FROM_PTR(val));
    for (;;) {
        __breakpoint();
    }
}

#ifndef NDEBUG
void MP_WEAK __assert_func(const char *file, int line, const char *func, const char *expr) {
    printf("Assertion '%s' failed, at file %s:%d\n", expr, file, line);
    panic("Assertion failed");
}
#endif

#define POLY (0xD5)

uint8_t rosc_random_u8(size_t cycles) {
    static uint8_t r;
    for (size_t i = 0; i < cycles; ++i) {
        r = ((r << 1) | rosc_hw->randombit) ^ (r & 0x80 ? POLY : 0);
        mp_hal_delay_us_fast(1);
    }
    return r;
}

uint32_t rosc_random_u32(void) {
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
        value = value << 8 | rosc_random_u8(32);
    }
    return value;
}
