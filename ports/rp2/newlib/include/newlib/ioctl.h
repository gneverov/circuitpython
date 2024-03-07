// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once


// Block device ioctls
// -------------------

// set device read-only (0 = read-write)
// param: const int *
#define BLKROSET 0x0000125D

// get read-only status (0 = read_write)
// param: int *
#define BLKROGET 0x0000125E

// return device size in 512-byte sectors
// param: unsigned long *
#define BLKGETSIZE 0x00001260

// flush buffer cache
// param: none
#define BLKFLSBUF 0x00001261

// get block device sector size in bytes
// param: int *
#define BLKSSZGET 0x00001268

// trim
// param: uint64_t[2] = { start, length }
// the start and length of the discard range in bytes
#define BLKDISCARD 0x00001277


int ioctl(int fd, unsigned long request, ...);
