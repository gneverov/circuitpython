// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/types.h>

// protection options
#define PROT_EXEC 0x04                  // Page can be executed.
#define PROT_NONE 0x00                  // Page cannot be accessed.
#define PROT_READ 0x01                  // Page can be read.
#define PROT_WRITE 0x02                 // Page can be written.

// flag options
#define MAP_FIXED 0x0400                 // Interpret addr exactly.
#define MAP_PRIVATE 0x0200               // Changes are private.
#define MAP_SHARED 0x0100                // Share changes.

#define MAP_FAILED NULL


void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);

int munmap(void *addr, size_t len);
