// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lwip/ip_addr.h"

#include "py/obj.h"


extern const mp_obj_type_t netif_type;

extern const mp_obj_type_t netif_list_type;

MP_DECLARE_CONST_FUN_OBJ_1(netif_getattr_obj);

MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(netif_dns_servers_obj);

mp_obj_t netif_inet_ntoa(const ip_addr_t *ipaddr);
void netif_inet_aton(mp_obj_t addr_in, ip_addr_t *ipaddr);
