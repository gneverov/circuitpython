// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <newlib/ioctl.h>

// mtd_info type
#define MTD_ABSENT 0
#define MTD_RAM 1
#define MTD_ROM 2
#define MTD_NORFLASH 3
#define MTD_NANDFLASH 4

// mtd_info flags
#define MTD_WRITEABLE       0x0400      // Device is writeable
#define MTD_BIT_WRITEABLE   0x0800      // Single bits can be flipped
#define MTD_NO_ERASE        0x1000      // No erase necessary
#define MTD_POWERUP_LOCK    0x2000      // Always locked after reset

// mtd_info flag combinations
#define MTD_CAP_ROM         0
#define MTD_CAP_RAM         (MTD_WRITEABLE | MTD_BIT_WRITEABLE | MTD_NO_ERASE)
#define MTD_CAP_NORFLASH    (MTD_WRITEABLE | MTD_BIT_WRITEABLE)
#define MTD_CAP_NANDFLASH   (MTD_WRITEABLE)


struct mtd_info {
    uint8_t type;
    uint32_t flags;
    uint32_t size;                      // Total size of the MTD in bytes
    uint32_t erasesize;                 // Erase sector size in bytes
    uint32_t writesize;                 // Write sector size in bytes
    uint32_t oobsize;                   // Amount of OOB data per block (e.g. 16)
};

struct erase_info {
    uint32_t start;
    uint32_t length;
};

struct mtd_oob_buf {
    uint32_t start;
    uint32_t length;
    char *ptr;
};
