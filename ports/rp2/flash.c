// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include "newlib/flash.h"

#include "hardware/flash.h"
#include "hardware/gpio.h"
#if !PICO_RP2040
#include "hardware/gpio.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip.h"
#include "hardware/sync.h"
#endif

#define MIN_FLASH_SIZE (2u << 20)
#define DEFAULT_FLASH_SIZE (16u << 20)


size_t flash_size;
size_t psram_size;
size_t flash_storage_offset;
size_t flash_storage_size;

static void read_flash_size(void) {
    char *flash_size_str = getenv("FLASH_SIZE");
    if (flash_size_str) {
        char *end;
        flash_size = strtoul(flash_size_str, &end, 0);
        if (!*end) {
            return;
        }
        flash_size = 0;
    }

    uint8_t jedec_id[4] = { 0x9f, 0x00 };
    flash_do_cmd(jedec_id, jedec_id, 4);
    if (jedec_id[1] == 0xef) {
        flash_size = 1u << jedec_id[3];
        return;
    }
}

#if !PICO_RP2040
// SPDX-SnippetBegin
// SPDX-SnippetCopyrightText: Copyright (c) 2021 Scott Shawcroft for Adafruit Industries
// SPDX-License-Identifier: MIT
// https://github.com/adafruit/circuitpython/blob/main/ports/raspberrypi/supervisor/port.c

static size_t __no_inline_not_in_flash_func(probe_psram)(uint csn) {
    size_t _psram_size = 0;
    uint32_t status = save_and_disable_interrupts();

    // Try and read the PSRAM ID via direct_csr.
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB |
        QMI_DIRECT_CSR_EN_BITS;
    // Need to poll for the cooldown on the last XIP transfer to expire
    // (via direct-mode BUSY flag) before it is safe to perform the first
    // direct-mode operation
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
    }

    // Exit out of QMI in case we've inited already
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    // Transmit as quad.
    qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS |
        QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB |
        0xf5;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
    }
    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);

    // Read the id
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    uint8_t kgd = 0;
    uint8_t eid = 0;
    for (size_t i = 0; i < 7; i++) {
        if (i == 0) {
            qmi_hw->direct_tx = 0x9f;
        } else {
            qmi_hw->direct_tx = 0xff;
        }
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS) == 0) {
        }
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
        }
        if (i == 5) {
            kgd = qmi_hw->direct_rx;
        } else if (i == 6) {
            eid = qmi_hw->direct_rx;
        } else {
            (void)qmi_hw->direct_rx;
        }
    }
    // Disable direct csr.
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    if (kgd == 0x5D) {
        _psram_size = 1024 * 1024; // 1 MiB
        uint8_t size_id = eid >> 5;
        if (eid == 0x26 || size_id == 2) {
            _psram_size *= 8;
        } else if (size_id == 0) {
            _psram_size *= 2;
        } else if (size_id == 1) {
            _psram_size *= 4;
        }
    }

    restore_interrupts(status);
    return _psram_size;
}

static void __no_inline_not_in_flash_func(setup_psram)(void) {
    uint32_t status = save_and_disable_interrupts();

    // Enable quad mode.
    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB |
        QMI_DIRECT_CSR_EN_BITS;
    // Need to poll for the cooldown on the last XIP transfer to expire
    // (via direct-mode BUSY flag) before it is safe to perform the first
    // direct-mode operation
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
    }

    // RESETEN, RESET and quad enable
    for (uint8_t i = 0; i < 3; i++) {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        if (i == 0) {
            qmi_hw->direct_tx = 0x66;
        } else if (i == 1) {
            qmi_hw->direct_tx = 0x99;
        } else {
            qmi_hw->direct_tx = 0x35;
        }
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0) {
        }
        qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);
        for (size_t j = 0; j < 20; j++) {
            asm ("nop");
        }
        (void)qmi_hw->direct_rx;
    }
    // Disable direct csr.
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    qmi_hw->m[1].timing =
        QMI_M0_TIMING_PAGEBREAK_VALUE_1024 << QMI_M0_TIMING_PAGEBREAK_LSB | // Break between pages.
            3 << QMI_M0_TIMING_SELECT_HOLD_LSB | // Delay releasing CS for 3 extra system cycles.
            1 << QMI_M0_TIMING_COOLDOWN_LSB |
            1 << QMI_M0_TIMING_RXDELAY_LSB |
            16 << QMI_M0_TIMING_MAX_SELECT_LSB | // In units of 64 system clock cycles. PSRAM says 8us max. 8 / 0.00752 / 64 = 16.62
            7 << QMI_M0_TIMING_MIN_DESELECT_LSB | // In units of system clock cycles. PSRAM says 50ns.50 / 7.52 = 6.64
            2 << QMI_M0_TIMING_CLKDIV_LSB;
    qmi_hw->m[1].rfmt = (QMI_M0_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_PREFIX_WIDTH_LSB |
            QMI_M0_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_RFMT_ADDR_WIDTH_LSB |
            QMI_M0_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_SUFFIX_WIDTH_LSB |
            QMI_M0_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_RFMT_DUMMY_WIDTH_LSB |
            QMI_M0_RFMT_DUMMY_LEN_VALUE_24 << QMI_M0_RFMT_DUMMY_LEN_LSB |
            QMI_M0_RFMT_DATA_WIDTH_VALUE_Q << QMI_M0_RFMT_DATA_WIDTH_LSB |
            QMI_M0_RFMT_PREFIX_LEN_VALUE_8 << QMI_M0_RFMT_PREFIX_LEN_LSB |
            QMI_M0_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M0_RFMT_SUFFIX_LEN_LSB);
    qmi_hw->m[1].rcmd = 0xeb << QMI_M0_RCMD_PREFIX_LSB |
        0 << QMI_M0_RCMD_SUFFIX_LSB;
    qmi_hw->m[1].wfmt = (QMI_M0_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_PREFIX_WIDTH_LSB |
            QMI_M0_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_WFMT_ADDR_WIDTH_LSB |
            QMI_M0_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_SUFFIX_WIDTH_LSB |
            QMI_M0_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_WFMT_DUMMY_WIDTH_LSB |
            QMI_M0_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M0_WFMT_DUMMY_LEN_LSB |
            QMI_M0_WFMT_DATA_WIDTH_VALUE_Q << QMI_M0_WFMT_DATA_WIDTH_LSB |
            QMI_M0_WFMT_PREFIX_LEN_VALUE_8 << QMI_M0_WFMT_PREFIX_LEN_LSB |
            QMI_M0_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M0_WFMT_SUFFIX_LEN_LSB);
    qmi_hw->m[1].wcmd = 0x38 << QMI_M0_WCMD_PREFIX_LSB |
        0 << QMI_M0_WCMD_SUFFIX_LSB;

    restore_interrupts(status);

    // Mark that we can write to PSRAM.
    xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;

    // Test write to the PSRAM.
    volatile uint32_t *psram_nocache = (volatile uint32_t *)0x15000000;
    psram_nocache[0] = 0x12345678;
    volatile uint32_t readback = psram_nocache[0];
    if (readback != 0x12345678) {
        psram_size = 0;
    }
}
// SPDX-SnippetEnd

static int read_psram_cs(void) {
    uint psram_cs;
    char *psram_cs_str = getenv("PSRAM_CS");
    if (psram_cs_str) {
        char *end;
        psram_cs = strtoul(psram_cs_str, &end, 10);
        if (*end || (psram_cs >= NUM_BANK0_GPIOS)) {
            return -1;
        }
        gpio_set_function(psram_cs, GPIO_FUNC_XIP_CS1);
        return psram_cs;
    }

    const uint csn[] = { 0, 8, 19, 47 };
    for (size_t i = 0; i < 4; i++) {
        gpio_set_function(csn[i], GPIO_FUNC_XIP_CS1);
        if (probe_psram(csn[i])) {
            psram_cs = csn[i];
            char psram_cs_str[4];
            sprintf(psram_cs_str, "%u", psram_cs);
            setenv("PSRAM_CS", psram_cs_str, 0);
            return psram_cs;
        }
        gpio_init(csn[i]);
    }
    setenv("PSRAM_CS", "", 0);
    return -1;
}

static void read_psram_size(uint psram_cs) {
    char *psram_size_str = getenv("PSRAM_SIZE");
    if (psram_size_str) {
        char *end;
        psram_size = strtoul(psram_size_str, &end, 0);
        if (!*end) {
            return;
        }
        psram_size = 0;
    }

    psram_size = probe_psram(psram_cs);
}
#endif

static void read_storage_size(void) {
    char *disk_size_str = getenv("DISK_SIZE");
    size_t disk_size = flash_size / 4;
    if (disk_size_str) {
        char *end;
        disk_size = strtoul(disk_size_str, &end, 0);
        if (*end) {
            disk_size = flash_size / 4;
        }
    }

    extern uint8_t __flash_heap_start[];
    while ((flash_size - disk_size) < ((uintptr_t)__flash_heap_start - XIP_BASE)) {
        disk_size /= 2;
    }

    flash_storage_offset = flash_size - disk_size;
    flash_storage_size = disk_size;
}

__attribute__((visibility("hidden")))
void flash_init(void) {
    read_flash_size();
    if (flash_size) {
        flash_size = MAX(flash_size, MIN_FLASH_SIZE);
    } else {
        flash_size = DEFAULT_FLASH_SIZE;
    }

    #if !PICO_RP2040
    int psram_cs = read_psram_cs();
    if (psram_cs >= 0) {
        read_psram_size(psram_cs);
        setup_psram();
    }
    #endif

    read_storage_size();
}

void flash_memread(uint32_t flash_offs, void *mem, size_t size) {
    hard_assert(flash_offs + size < flash_size);
    memcpy(mem, (void *)(flash_offs + XIP_NOCACHE_NOALLOC_BASE), size);
}

void flash_memwrite(uint32_t flash_offs, const void *mem, size_t size) {
    hard_assert(flash_offs + size < flash_size);
    flash_range_erase(flash_offs, size & -FLASH_SECTOR_SIZE);
    flash_range_program(flash_offs, mem, size & -FLASH_PAGE_SIZE);
}
