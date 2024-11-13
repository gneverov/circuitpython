// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <stdarg.h>

#define IOCTL_MAX_ARG_SIZE 64


// Block device ioctls
// -------------------
#define BLK_BASE 0x0200

// set device read-only (0 = read-write)
// param: const int *
#define BLKROSET    (BLK_BASE + 0)

// get read-only status (0 = read_write)
// param: int *
#define BLKROGET    (BLK_BASE + 1)

// return device size in 512-byte sectors
// param: unsigned long *
#define BLKGETSIZE  (BLK_BASE + 2)

// flush buffer cache
// param: none
#define BLKFLSBUF   (BLK_BASE + 3)

// get block device sector size in bytes
// param: int *
#define BLKSSZGET   (BLK_BASE + 4)

// trim
// param: uint64_t[2] = { start, length }
// the start and length of the discard range in bytes
#define BLKDISCARD  (BLK_BASE + 5)


// Terminal device ioctls
// ---
#define TC_BASE 0x0100
#define TCFLSH      (TC_BASE + 0)       // Equivalent to tcflush(fd, arg)
#define TCGETS      (TC_BASE + 1)       // Equivalent to tcgetattr(fd, arg)
#define TCSBRK      (TC_BASE + 2)       // Equivalent to tcsendbreak(fd, arg)
#define TCSETS      (TC_BASE + 3)       // Equivalent to tcsetattr(fd, TCSANOW, arg)
#define TCXONC      (TC_BASE + 4)       // Equivalent to tcflow(fd, arg)
#define TIOCGWINSZ  (TC_BASE + 5)       // Get window size
#define TIOCSWINSZ  (TC_BASE + 6)       // Set window size


// MTD device ioctls
// ---
#define MEM_BASE 0x0300

// Get basic MTD characteristics info
// param: struct mtd_info *
#define MEMGETINFO  (MEM_BASE + 0)

// Erase segment of MTD
// param: const struct erase_info *
#define MEMERASE    (MEM_BASE + 1)

// Write out-of-band data from MTD
// param: const struct mtd_oob_buf *
#define MEMWRITEOOB (MEM_BASE + 2)

// Read out-of-band data from MTD
// param: struct mtd_oob_buf *
#define MEMREADOOB  (MEM_BASE + 3)

// Lock a chip (for MTD that supports it)
// param: none
#define MEMLOCK     (MEM_BASE + 4)

// Unlock a chip (for MTD that supports it)
// param: none
#define MEMUNLOCK   (MEM_BASE + 5)


// MMC device ioctls
// ---
#define MMC_BASE 0x0400

#define MMC_IOC_CMD (MMC_BASE + 0)


// Functions
// ---
int ioctl(int fd, unsigned long request, ...);

int vioctl(int fd, unsigned long request, va_list args);
