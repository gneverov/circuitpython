// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "morelib/ring.h"

#include "py/obj.h"


typedef struct {
    mp_obj_base_t base;
    int fd;
    mp_obj_t name;
    mp_obj_t mode;
    bool closefd;
} mp_obj_io_file_t;

typedef struct {
    mp_obj_base_t base;
    mp_obj_t stream;
    int isatty : 1;
    ring_t in_buffer;
} mp_obj_io_text_t;


extern const mp_obj_type_t mp_type_io_fileio;
extern const mp_obj_type_t mp_type_io_textio;

void mp_io_print(void *data, const char *str, size_t len);
