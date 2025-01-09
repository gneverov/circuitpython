// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <morelib/ping.h>

#include "extmod/modos_newlib.h"
#include "extmod/socket/netif.h"
#include "extmod/socket/ping.h"
#include "py/runtime.h"


extern const mp_obj_module_t mp_module_socket;

static mp_obj_t ping_ping(mp_obj_t dest_in) {
    mp_obj_t dest[] = { NULL, NULL, dest_in };
    mp_load_method(MP_OBJ_FROM_PTR(&mp_module_socket), MP_QSTR_gethostbyname, dest);
    mp_obj_t addr_in = mp_call_method_n_kw(1, 0, dest);
    ip_addr_t ipaddr;
    netif_inet_aton(addr_in, &ipaddr);
    int ret;
    MP_OS_CALL(ret, ping, &ipaddr);
    mp_os_check_ret(ret);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(ping_ping_obj, ping_ping);
