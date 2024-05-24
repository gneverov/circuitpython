// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <sys/types.h>


struct statvfs {
    // File system block size.
    unsigned long f_bsize;

    // Fundamental file system block size.
    unsigned long f_frsize;

    // Total number of blocks on file system in units of f_frsize.
    fsblkcnt_t f_blocks;

    // Total number of free blocks.
    fsblkcnt_t f_bfree;

    // Number of free blocks available to non-privileged process.
    fsblkcnt_t f_bavail;

    // Total number of file serial numbers.
    fsfilcnt_t f_files;

    // Total number of free file serial numbers.
    fsfilcnt_t f_ffree;

    // Number of file serial numbers available to non-privileged process.
    fsfilcnt_t f_favail;

    // File system ID.
    unsigned long f_fsid;

    // Bit mask of f_flag values.
    unsigned long f_flag;

    // Maximum filename length.
    unsigned long f_namemax;
};

enum {
    ST_RDONLY = 1,              /* Mount read-only.  */
    ST_NOSUID = 2               /* Ignore suid and sgid bits.  */
};

int fstatvfs(int, struct statvfs *);

int statvfs(const char *, struct statvfs *);
