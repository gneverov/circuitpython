// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/stat.h>

enum {
    DT_UNKNOWN = 0,
    DT_FIFO = S_IFIFO,
    DT_CHR = S_IFCHR,
    DT_DIR = S_IFDIR,
    DT_BLK = S_IFBLK,
    DT_REG = S_IFREG,
    DT_LNK = S_IFLNK,
    DT_SOCK = S_IFSOCK,
};

typedef void DIR;

struct dirent {
    ino_t d_ino;
    unsigned char d_type;
    char *d_name;
};
