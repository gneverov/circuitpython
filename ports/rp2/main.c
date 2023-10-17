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

#include <malloc.h>
#include <signal.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

#include "freertos/task_helper.h"
#include "lwip/lwip_init.h"
#include "newlib/newlib.h"
#include "pico/dma.h"
#include "pico/gpio.h"
#include "pico/pio.h"
#include "pico/terminal.h"
#include "tinyusb/net_device_lwip.h"
#include "tinyusb/terminal.h"
#include "tinyusb/tusb_config.h"
#include "tinyusb/tusb_lock.h"

#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
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
#if MICROPY_PY_NETWORK_CYW43
#include "pico/cyw43_driver.h"
#endif


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

        // Execute _boot.py to set up the filesystem.
        #if MICROPY_VFS_FAT
        pyexec_frozen_module("_boot_fat.py", false);
        #else
        pyexec_frozen_module("_boot.py", false);
        #endif

        // Execute user scripts.
        int ret = pyexec_file_if_exists("boot.py");
        if (ret & PYEXEC_FORCED_EXIT) {
            goto soft_reset_exit;
        }
        if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
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
        mp_printf(MP_PYTHON_PRINTER, "MPY: soft reboot\n");
        signal_deinit();
        #if MICROPY_PY_BLUETOOTH
        mp_bluetooth_deinit();
        #endif
        machine_pwm_deinit_all();
        #if MICROPY_PY_THREAD
        mp_thread_deinit();
        #endif
        gc_sweep_all();
        mp_deinit();
    }
}

StaticTask_t mp_taskdef;
const volatile __in_flash("gc_heap_size") size_t mp_gc_heap_size = 96 << 10;

void mp_task(void *params) {
    task_init();
    size_t gc_heap_size = mp_gc_heap_size;
    uint8_t *gc_heap = malloc(gc_heap_size);
    while (!gc_heap) {
        gc_heap_size /= 2;
        gc_heap = malloc(gc_heap_size);
    }

    fd_close();
    bool has_terminal = false;

    #if MICROPY_HW_USB_CDC
    const tusb_config_t *tusb_config = tusb_config_get();
    if (!has_terminal && tusb_config && tusb_config->device && (tusb_config->cdc_itf != 255) && !tusb_config->disconnect) {
        while (!tud_inited()) {
            portYIELD();
        }
        has_terminal = !terminal_usb_open(tusb_config->cdc_itf);
    }
    #endif

    #if MICROPY_HW_ENABLE_UART_REPL
    if (!has_terminal) {
        has_terminal = !terminal_uart_open();
    }
    #endif
    assert(has_terminal);

    mp_main((uint8_t *)&__MpStackBottom, (uint8_t *)&__MpStackTop, gc_heap, gc_heap + gc_heap_size);

    free(gc_heap);
    task_deinit();
    vTaskDelete(NULL);
}

#if CFG_TUD_ENABLED
void mp_tud_task(void *params) {
    const tusb_config_t *tusb_config = tusb_config_get();

    tud_lock_init();
    tud_init(TUD_OPT_RHPORT);

    if (!tusb_config || tusb_config->disconnect) {
        tud_disconnect();
    }

    #if CFG_TUD_ECM_RNDIS || CFG_TUD_NCM
    lwip_wait();
    tud_network_init();
    #endif

    while (1) {
        tud_task();
    }
    vTaskDelete(NULL);
}
#endif

#if CFG_TUH_ENABLED
void mp_tuh_task(void *params) {
    // const tusb_config_t *tusb_config = tusb_config_get();

    tuh_init(TUH_OPT_RHPORT);

    while (1) {
        tuh_task();
    }
    vTaskDelete(NULL);
}
#endif

int main(int argc, char **argv) {
    pico_dma_init();
    pico_gpio_init();
    pico_pio_init();

    terminal_boot_open();

    #if MICROPY_PY_NETWORK_CYW43
    cyw43_driver_init();
    #endif

    lwip_helper_init();

    const tusb_config_t *tusb_config = tusb_config_get();
    #if CFG_TUD_ENABLED
    if (tusb_config && tusb_config->device) {
        xTaskCreate(mp_tud_task, "tud", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    }
    #endif
    #if CFG_TUH_ENABLED
    if (tusb_config && tusb_config->host) {
        xTaskCreate(mp_tuh_task, "tuh", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    }
    #endif

    xTaskCreateStatic(mp_task, "mp", (&__MpStackTop - &__MpStackBottom) / sizeof(StackType_t), NULL, 1, (StackType_t *)&__MpStackBottom, &mp_taskdef);
    vTaskStartScheduler();
    return 0;
}

void gc_collect(void) {
    gc_collect_start();
    gc_helper_collect_regs_and_stack();
    #if MICROPY_PY_THREAD
    mp_thread_gc_others();
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
