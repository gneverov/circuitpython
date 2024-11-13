// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/stat.h>
#include <sys/types.h>


struct dev_driver {
    dev_t dev;
    void *(*open)(const void *ctx, dev_t dev, int flags, mode_t mode);
    int (*stat)(const void *ctx, dev_t dev, struct stat *pstat);
};

extern const struct dev_driver *dev_drvs[];
extern const size_t dev_num_drvs;

inline dev_t makedev(unsigned int maj, unsigned int min) {
    return ((maj & 0xff) << 8) | (min & 0xff);
}
// #define makedev(maj, min) ((((maj) & 0xff) << 8) | ((min) & 0xff))

inline unsigned int major(dev_t dev) {
    return (dev >> 8) & 0xff;
}
// #define major(dev) (((dev) >> 8) & 0xff)

inline unsigned int minor(dev_t dev) {
    return dev & 0xff;
}
// #define minor(dev) ((dev) & 0xff)

int opendev(dev_t dev, int flags, mode_t mode);

int statdev(const void *ctx, dev_t dev, struct stat *pstat);

void dev_lock(void);

void dev_unlock(void);

// Standard device umbers
enum {
    DEV_MEM = 0x0101,
    DEV_NULL = 0x0103,
    DEV_ZERO = 0x0105,
    DEV_FULL = 0x0107,
    DEV_RANDOM = 0x0108,
    DEV_URANDOM = 0x0109,

    DEV_TTY0 = 0x0400,
    DEV_TTY1 = 0x0401,
    DEV_TTY2 = 0x0402,
    DEV_TTY3 = 0x0403,

    DEV_TTYS0 = 0x0440,
    DEV_TTYS1 = 0x0441,

    DEV_TTY = 0x0500,
    DEV_CONSOLE = 0x0501,
    DEV_PTMX = 0x0502,

    DEV_MTD0 = 0x5A00,
    DEV_MTD1 = 0x5A01,
    DEV_MTD2 = 0x5A02,
    DEV_MTD3 = 0x5A03,

    DEV_MMCBLK0 = 0xb300,
    DEV_MMCBLK1 = 0xb301,
    DEV_MMCBLK2 = 0xb302,
    DEV_MMCBLK3 = 0xb303,

    DEV_TTYUSB0 = 0xbc00,
    DEV_TTYUSB1 = 0xbc01,
    DEV_TTYUSB2 = 0xbc02,
    DEV_TTYUSB3 = 0xbc03,

    DEV_MTDBLK0 = 0xEA00,
    DEV_MTDBLK1 = 0xEA01,
    DEV_MTDBLK2 = 0xEA02,
    DEV_MTDBLK3 = 0xEA03,

    DEV_UF2 = 0xf000,
};
