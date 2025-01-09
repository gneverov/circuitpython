// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <sys/socket.h>

#include "lwip/err.h"

#include "py/obj.h"


void mp_socket_lwip_raise(err_t err);

socklen_t mp_socket_sockaddr_parse(mp_obj_t address_in, struct sockaddr_storage *address);

mp_obj_t mp_socket_sockaddr_format(const struct sockaddr *address, socklen_t address_len);

typedef struct {
    mp_obj_base_t base;
    int fd;
    int timeout;
} mp_obj_socket_t;

extern const mp_obj_type_t mp_type_socket;
