// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "./netif.h"
#include "./ping.h"
#include "./sntp.h"


static const mp_rom_map_elem_t network_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_network) },
    { MP_ROM_QSTR(MP_QSTR_NetInterface),    MP_ROM_PTR(&netif_type) },
    { MP_ROM_QSTR(MP_QSTR___getattr__),     MP_ROM_PTR(&netif_getattr_obj) },
    { MP_ROM_QSTR(MP_QSTR_dns_servers),     MP_ROM_PTR(&netif_dns_servers_obj) },
    { MP_ROM_QSTR(MP_QSTR_ping),            MP_ROM_PTR(&ping_ping_obj) },
    { MP_ROM_QSTR(MP_QSTR_sntp),            MP_ROM_PTR(&sntp_sntp_obj) },
};
static MP_DEFINE_CONST_DICT(network_module_globals, network_module_globals_table);

const mp_obj_module_t network_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&network_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_network, network_module);
