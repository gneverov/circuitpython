// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/obj.h"

void parse_args(size_t n_args, const mp_obj_t *args, const char *format, ...);

void parse_args_and_kw(size_t n_args, size_t n_kw, const mp_obj_t *args, const char *format, const qstr keywords[], ...);

void parse_args_and_kw_map(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args, const char *format, const qstr keywords[], ...);
