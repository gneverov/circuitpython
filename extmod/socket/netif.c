// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdarg.h>
#include <string.h>

#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/netif.h"

#include "./error.h"
#include "./netif.h"
#include "./socket.h"
#include "shared/netutils/netutils.h"
#include "py/gc.h"
#include "py/mperrno.h"
#include "py/obj.h"
#include "py/qstr.h"
#include "py/runtime.h"


typedef struct {
    mp_obj_base_t base;
    u8_t index;
    mp_stream_poll_t poll;
} netif_obj_t;

typedef err_t (*netif_func_t)(struct netif *netif, va_list args);

STATIC err_t netif_vcall(u8_t index, netif_func_t func, va_list args) {
    err_t err;
    LOCK_TCPIP_CORE();
    struct netif *netif = netif_get_by_index(index);
    if (netif) {
        err = func(netif, args);
    }
    else {
        err = ERR_ARG;
    }
    UNLOCK_TCPIP_CORE();
    return err;
}

err_t netif_call(u8_t index, netif_func_t func, ...) {
    va_list args;
    va_start(args, func);
    return netif_vcall(index, func, args);
}

void netif_call_raise(u8_t index, netif_func_t func, ...) {
    va_list args;
    va_start(args, func);
    err_t err = netif_vcall(index, func, args);
    socket_lwip_raise(err);
}

STATIC u8_t netif_lwip_client_id() {
    static u8_t id = 0;
    if (id == 0) {
        id = netif_alloc_client_data_id();
    }
    return id;
}

STATIC err_t netif_lwip_get(struct netif *netif, va_list args) {
    netif_obj_t **self = va_arg(args, netif_obj_t **);
    *self = netif_get_client_data(netif, netif_lwip_client_id());
    return ERR_OK;
}

STATIC err_t netif_lwip_set(struct netif *netif, va_list args) {
    netif_obj_t *self = va_arg(args, netif_obj_t *);
    netif_set_client_data(netif, netif_lwip_client_id(), self);
    return ERR_OK;
}

STATIC mp_obj_t netif_make_new(const mp_obj_type_t *type, size_t n_args, const mp_obj_t *args) {
    mp_arg_check_num(n_args, 0, 1, 1, false);
    u8_t index = mp_obj_get_int(args[0]);

    netif_obj_t *self;
    netif_call_raise(index, netif_lwip_get, &self);

    if (!self) {
        self = m_new_obj_with_finaliser(netif_obj_t);
        self->base.type = type;
        self->index = index;
        mp_stream_poll_init(&self->poll);
        netif_call_raise(index, netif_lwip_set, self);
    }

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t netif_new(u8_t index) {
    mp_obj_t args[] = { MP_OBJ_NEW_SMALL_INT(index) };
    return netif_make_new(&netif_type, 1, args); 
}

STATIC mp_obj_t netif_del(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    netif_call(self->index, netif_lwip_set, NULL);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(netif_del_obj, netif_del);

STATIC err_t netif_lwip_dict(struct netif *netif, va_list args) {
    struct netif *netif_copy = va_arg(args, struct netif *);
    char *name = va_arg(args, char *);
    *netif_copy = *netif;
    netif_index_to_name(netif_get_index(netif), name);
    return ERR_OK;
}

STATIC mp_obj_t netif_dict(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    struct netif netif;
    char name[NETIF_NAMESIZE];
    netif_call_raise(self->index, netif_lwip_dict, &netif, name);

    mp_obj_t dict = mp_obj_new_dict(16);
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_index), MP_OBJ_NEW_SMALL_INT(netif_get_index(&netif)));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_address), netutils_format_ipv4_addr((uint8_t *)netif_ip_addr4(&netif), NETUTILS_BIG));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_gateway), netutils_format_ipv4_addr((uint8_t *)netif_ip_gw4(&netif), NETUTILS_BIG));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_netmask), netutils_format_ipv4_addr((uint8_t *)netif_ip_netmask4(&netif), NETUTILS_BIG));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_dhcp), mp_obj_new_bool(dhcp_supplied_address(&netif)));

    vstr_t hwaddr;
    vstr_init(&hwaddr, ETH_HWADDR_LEN * 3);
    for (size_t i = 0; i < netif.hwaddr_len; i++) {
        vstr_printf(&hwaddr, "%02x:", netif.hwaddr[i]);
    }
    vstr_cut_tail_bytes(&hwaddr, 1);
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_mac), mp_obj_new_str_from_vstr(&hwaddr));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_mtu), MP_OBJ_NEW_SMALL_INT(netif.mtu));

    const char *hostname = netif_get_hostname(&netif);
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_hostname), mp_obj_new_str(hostname, strlen(hostname)));

    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_enabled), mp_obj_new_bool(netif_is_up(&netif)));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_link_up), mp_obj_new_bool(netif_is_link_up(&netif)));

    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_name), mp_obj_new_str(name, strlen(name)));

    return dict;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(netif_dict_obj, netif_dict);

STATIC void netif_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    if (gc_is_locked()) {
        // We are probably executing finalizers so we cannot allocate dict from GC heap.
        dest[1] = MP_OBJ_SENTINEL;
        return;
    }

    mp_obj_t dict = netif_dict(self_in);
    if (attr == MP_QSTR___dict__) {
        dest[0] = dict;
        return;
    }

    mp_map_t *map = mp_obj_dict_get_map(dict);
    mp_map_elem_t *elem = mp_map_lookup(map, MP_OBJ_NEW_QSTR(attr), MP_MAP_LOOKUP);
    if (elem) {
        dest[0] = elem->value;
     }
     else {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

STATIC void netif_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    struct netif netif;
    char name[NETIF_NAMESIZE];
    netif_call_raise(self->index, netif_lwip_dict, &netif, name);

    char address[IP4ADDR_STRLEN_MAX];
    ip4addr_ntoa_r(netif_ip_addr4(&netif), address, IP4ADDR_STRLEN_MAX);
    mp_printf(print, "NetInterface(name=%s, address=%s, link=%s)", name, address, netif_is_link_up(&netif) ? "up" : "down");
}

STATIC err_t netif_lwip_configure(struct netif *netif, va_list args) {
    ip_addr_t *address = va_arg(args, ip4_addr_t *);
    ip_addr_t *gateway = va_arg(args, ip4_addr_t *);
    ip_addr_t *netmask = va_arg(args, ip4_addr_t *);
    netif_set_addr(netif, address, gateway, netmask);
    dhcp_inform(netif);
    return ERR_OK;
}

STATIC mp_obj_t netif_configure(size_t n_args, const mp_obj_t *args) {
    netif_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    ip_addr_t address, gateway, netmask;
    netutils_parse_ipv4_addr(args[1], (uint8_t *)&address, NETUTILS_BIG);
    netutils_parse_ipv4_addr(args[2], (uint8_t *)&gateway, NETUTILS_BIG);
    netutils_parse_ipv4_addr(args[3], (uint8_t *)&netmask, NETUTILS_BIG);

    netif_call_raise(self->index, netif_lwip_configure, &address, &gateway, &netmask);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(netif_configure_obj, 4, 4, netif_configure);

STATIC err_t netif_lwip_dhcp_start(struct netif *netif, va_list args) {
    return dhcp_start(netif);
}

STATIC mp_obj_t netif_dhcp_start(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    netif_call_raise(self->index, netif_lwip_dhcp_start);
    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(netif_dhcp_start_obj, netif_dhcp_start);

STATIC err_t netif_lwip_dhcp_stop(struct netif *netif, va_list args) {
    dhcp_release_and_stop(netif);
    return ERR_OK;
}

STATIC mp_obj_t netif_dhcp_stop(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    netif_call_raise(self->index, netif_lwip_dhcp_stop);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(netif_dhcp_stop_obj, netif_dhcp_stop);

STATIC err_t netif_lwip_dhcp_renew(struct netif *netif, va_list args) {
    return dhcp_renew(netif);
}

STATIC mp_obj_t netif_dhcp_renew(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    netif_call_raise(self->index, netif_lwip_dhcp_renew);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(netif_dhcp_renew_obj, netif_dhcp_renew);

STATIC err_t netif_lwip_enable(struct netif *netif, va_list args) {
    int enable = va_arg(args, int);
    if (enable) {
        netif_set_up(netif);
    }
    else {
        dhcp_release_and_stop(netif);
        netif_set_down(netif);
    }
    return ERR_OK;
}

STATIC mp_obj_t netif_enable(mp_obj_t self_in, mp_obj_t enable_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int enable = mp_obj_is_true(enable_in);
    netif_call_raise(self->index, netif_lwip_enable, enable);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(netif_enable_obj, netif_enable);

STATIC void netif_lwip_status_callback(struct netif *netif) {
    netif_obj_t *self = netif_get_client_data(netif, netif_lwip_client_id());
    if (self) {
        mp_stream_poll_signal(&self->poll, MP_STREAM_POLL_RD, NULL);
    }
    netif_set_status_callback(netif, NULL);
}

STATIC err_t netif_lwip_wait(struct netif *netif, va_list args) {
    if (ip_addr_isany(netif_ip_addr4(netif)) || !netif_is_link_up(netif)) {
        netif_set_status_callback(netif, netif_lwip_status_callback);
        return ERR_WOULDBLOCK;
    }
    return ERR_OK;
}

STATIC mp_uint_t netif_wait_nonblock(mp_obj_t stream_obj, void *buf, mp_uint_t len, int *errcode) {
    netif_obj_t *self = MP_OBJ_TO_PTR(stream_obj);
    err_t err = netif_call(self->index, netif_lwip_wait);
    return socket_lwip_err(err, errcode) ? MP_STREAM_ERROR : 0;
}

STATIC mp_obj_t netif_wait(size_t n_args, const mp_obj_t *args) {
    TickType_t timeout = portMAX_DELAY;
    if ((n_args > 1) && (args[1] != mp_const_none)) {
        timeout = pdMS_TO_TICKS(mp_obj_get_int(args[1]));
    }

    int errcode;
    if (mp_poll_block(args[0], NULL, 0, &errcode, netif_wait_nonblock, MP_STREAM_POLL_RD, timeout, false) == MP_STREAM_ERROR) {
        mp_raise_OSError(errcode);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(netif_wait_obj, 1, 2, netif_wait);

STATIC mp_uint_t netif_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    uint32_t ret;
    switch (request) {
        case MP_STREAM_POLL_CTL:
            LOCK_TCPIP_CORE();
            ret = mp_stream_poll_ctl(&self->poll, (void*)arg, errcode);
            UNLOCK_TCPIP_CORE();
            break;
        default:
            *errcode = MP_EINVAL;
            ret = MP_STREAM_ERROR;
            break;
    }
    return ret;
}

STATIC const mp_rom_map_elem_t netif_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&netif_del_obj) },
    { MP_ROM_QSTR(MP_QSTR___dict__),        MP_ROM_PTR(&netif_dict_obj) },
    { MP_ROM_QSTR(MP_QSTR_configure),       MP_ROM_PTR(&netif_configure_obj) },
    { MP_ROM_QSTR(MP_QSTR_dhcp_start),      MP_ROM_PTR(&netif_dhcp_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_dhcp_stop),       MP_ROM_PTR(&netif_dhcp_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_dhcp_renew),      MP_ROM_PTR(&netif_dhcp_renew_obj) },
    { MP_ROM_QSTR(MP_QSTR_enable),          MP_ROM_PTR(&netif_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_wait),            MP_ROM_PTR(&netif_wait_obj) },
};
STATIC MP_DEFINE_CONST_DICT(netif_locals_dict, netif_locals_dict_table);

STATIC const mp_stream_p_t netif_stream_p = {
    .ioctl = netif_ioctl,
    .can_poll = 1,
};

MP_DEFINE_CONST_OBJ_TYPE(
    netif_type,
    MP_QSTR_NetInterface,
    MP_TYPE_FLAG_NONE,
    // make_new, netif_make_new,
    print, netif_print,
    attr, netif_attr,
    protocol, &netif_stream_p,
    locals_dict, &netif_locals_dict
    );


STATIC mp_obj_t netif_list_make_new(const mp_obj_type_t *type, size_t n_args, const mp_obj_t *args) {
    mp_arg_check_num(n_args, 0, 0, 0, false);
    mp_obj_base_t *netif_list = m_new_obj(mp_obj_base_t);
    netif_list->type = type;
    return MP_OBJ_FROM_PTR(netif_list);
}

STATIC err_t netif_lwip_set_default(struct netif *netif, va_list args) {
    netif_set_default(netif);
    return ERR_OK;
}

STATIC void netif_list_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    if (attr == MP_QSTR_default) {
        if (dest[0] != MP_OBJ_SENTINEL) {
            LOCK_TCPIP_CORE();
            u8_t index = netif_default ? netif_get_index(netif_default) : 0;
            UNLOCK_TCPIP_CORE();
            dest[0] = index ? netif_new(index) : mp_const_none;
        }
        else if (dest[1] != MP_OBJ_NULL) {
            if (mp_obj_get_type(dest[1]) != &netif_type) {
                mp_raise_TypeError(NULL);
            }
            netif_obj_t *self = MP_OBJ_TO_PTR(dest[1]);
            netif_call_raise(self->index, netif_lwip_set_default);
            dest[0] = MP_OBJ_NULL;
        }
        else {
            mp_raise_TypeError(NULL);
        }
    }
    else {
        dest[1] = MP_OBJ_SENTINEL;
    }
}

STATIC mp_obj_t netif_list_subscr(mp_obj_t self_in, mp_obj_t index_in, mp_obj_t value) {
    if (value != MP_OBJ_SENTINEL) {
        mp_raise_TypeError(NULL);
    }

    u8_t index = 0;
    const mp_obj_type_t *exc_type = &mp_type_TypeError;
    if (mp_obj_is_int(index_in)) {
        LOCK_TCPIP_CORE();
        index = MP_OBJ_SMALL_INT_VALUE(index_in);
        struct netif *netif = netif_get_by_index(index);
        index = netif ? netif_get_index(netif) : 0;
        UNLOCK_TCPIP_CORE();
        exc_type = &mp_type_IndexError;
    }
    else if(mp_obj_is_str(index_in)) {
        LOCK_TCPIP_CORE();
        const char *name = mp_obj_str_get_str(index_in);
        struct netif *netif = netif_find(name);
        index = netif ? netif_get_index(netif) : 0;
        UNLOCK_TCPIP_CORE();
        exc_type = &mp_type_KeyError;        
    }
    if (index == 0) {
        mp_raise_type(exc_type);
    }

    return netif_new(index);
}

STATIC mp_obj_t netif_list_tuple(mp_obj_t self_in) {
    uint32_t netif_mask = 0;
    LOCK_TCPIP_CORE();
    struct netif *netif = netif_list;
    while (netif) {
        netif_mask |= 1u << netif_get_index(netif);
        netif = netif->next;
    }
    UNLOCK_TCPIP_CORE();

    mp_obj_t netifs[32];
    size_t num_netifs = 0;
    for (int i = 0; i < 32; i++) {
        if (netif_mask & (1u << i)) {
            netifs[num_netifs++] = netif_new(i);
        }
    }
    return mp_obj_new_tuple(num_netifs, netifs);
}

STATIC mp_obj_t netif_list_getiter(mp_obj_t self_in,  mp_obj_iter_buf_t *iter_buf) {
    mp_obj_t tuple = netif_list_tuple(self_in);
    return mp_obj_tuple_getiter(tuple, iter_buf);
}

STATIC mp_obj_t netif_list_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
    if (op == MP_UNARY_OP_LEN) {
        mp_obj_t tuple = netif_list_tuple(self_in);
        size_t len;
        mp_obj_t *items;
        mp_obj_tuple_get(tuple, &len, &items);
        return MP_OBJ_NEW_SMALL_INT(len);
    }

    return MP_OBJ_NULL;
}

MP_DEFINE_CONST_OBJ_TYPE(
    netif_list_type,
    MP_QSTR_NetInterfaceCollection,
    MP_TYPE_FLAG_ITER_IS_GETITER,
    // make_new, netif_list_make_new,
    attr, netif_list_attr,
    unary_op, netif_list_unary_op,
    subscr, netif_list_subscr,
    iter, netif_list_getiter
    );

STATIC mp_obj_t netif_dns_servers() {
    ip_addr_t dns_servers[DNS_MAX_SERVERS];
    LOCK_TCPIP_CORE();
    for (size_t i = 0; i < DNS_MAX_SERVERS; i++) {
        dns_servers[i] = *dns_getserver(i);
    }
    UNLOCK_TCPIP_CORE();

    size_t len = 0;
    mp_obj_t items[DNS_MAX_SERVERS];
    for (size_t i = 0; i < DNS_MAX_SERVERS; i++) {
        if (ip_addr_isany(&dns_servers[i])) {
            continue;
        }
        items[len++] = netutils_format_ipv4_addr((uint8_t *)&dns_servers[i], NETUTILS_BIG);
    }
    return mp_obj_new_list(len, items);
}

STATIC mp_obj_t netif_getattr(mp_obj_t attr) {
    switch (MP_OBJ_QSTR_VALUE(attr)) {
        case MP_QSTR_netif:
            return netif_list_make_new(&netif_list_type, 0, NULL);
        case MP_QSTR_dns_servers:
            return netif_dns_servers();
        default:
            return MP_OBJ_NULL;
    }
}
MP_DEFINE_CONST_FUN_OBJ_1(netif_getattr_obj, netif_getattr);
