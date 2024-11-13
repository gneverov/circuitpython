// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "py/obj.h"


/*
Format string syntax
E.g., "s" is the character in the format string and "const char **str" is the type of the value in the vargs list.

s - const char **str
parses object to C string

s* - mp_buffer_info_t *buf
parses object to MP buffer

i - mp_int_t *val
parses object to integer

p - mp_int_t *val
parses object to boolean

O - mp_obj_t *obj
parses object to object

O! - const mp_obj_type_t *type, mp_obj_t *obj
checks object has specified type
takes 2 vargs: the object type (input) and the parsed obj (output)

O& - void *(*conv)(mp_obj_t), void **ret
converts object using converter function
converter function returns MP_OBJ_NULL on conversion failure
otherwise result of converter function is stored as output
takes 2 vargs: the conv function (input) and the parsed obj (output)

| -
following arguments are options
takes no vargs

$ -
following arguments are keyword-only
takes no vargs


See https://docs.python.org/3/c-api/arg.html for analogous functionality in CPython.
*/

void parse_args(size_t n_args, const mp_obj_t *args, const char *format, ...);

void parse_args_and_kw(size_t n_args, size_t n_kw, const mp_obj_t *args, const char *format, const qstr keywords[], ...);

void parse_args_and_kw_map(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args, const char *format, const qstr keywords[], ...);
