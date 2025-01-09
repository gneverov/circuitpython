// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdarg.h>
#include <string.h>

#include "lwip/dhcp.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netif.h"

#include "extmod/io/poll.h"
#include "extmod/modos_newlib.h"
#include "extmod/socket/netif.h"
#include "extmod/socket/socket.h"
#include "py/gc.h"
#include "py/runtime.h"


typedef struct {
    mp_obj_base_t base;
    mp_poll_t poll;
    u8_t index;
} netif_obj_t;

typedef err_t (*netif_func_t)(struct netif *netif, va_list args);

static void socket_lwip_raise(err_t err) {
    if (err != ERR_OK) {
        mp_raise_OSError(err_to_errno(err));
    }
}

static err_t netif_vcall(u8_t index, netif_func_t func, va_list args) {
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

static u8_t netif_lwip_client_id() {
    static u8_t id = 0;
    if (id == 0) {
        id = netif_alloc_client_data_id();
    }
    return id;
}

static err_t netif_lwip_get(struct netif *netif, va_list args) {
    netif_obj_t **self = va_arg(args, netif_obj_t **);
    *self = netif_get_client_data(netif, netif_lwip_client_id());
    return ERR_OK;
}

static err_t netif_lwip_set(struct netif *netif, va_list args) {
    netif_obj_t *self = va_arg(args, netif_obj_t *);
    netif_set_client_data(netif, netif_lwip_client_id(), self);
    return ERR_OK;
}

static mp_obj_t netif_make_new(const mp_obj_type_t *type, size_t n_args, const mp_obj_t *args) {
    mp_arg_check_num(n_args, 0, 1, 1, false);
    u8_t index = mp_obj_get_int(args[0]);

    netif_obj_t *self;
    netif_call_raise(index, netif_lwip_get, &self);

    if (!self) {
        self = mp_obj_malloc_with_finaliser(netif_obj_t, type);
        mp_poll_init(&self->poll);
        self->index = index;
        netif_call_raise(index, netif_lwip_set, self);
        mp_os_check_ret(mp_poll_alloc(&self->poll, 0));
    }
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t netif_new(u8_t index) {
    mp_obj_t args[] = { MP_OBJ_NEW_SMALL_INT(index) };
    return netif_make_new(&netif_type, 1, args); 
}

static mp_obj_t netif_del(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    netif_call(self->index, netif_lwip_set, NULL);
    mp_poll_deinit(&self->poll);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(netif_del_obj, netif_del);

static err_t netif_lwip_dict(struct netif *netif, va_list args) {
    struct netif *netif_copy = va_arg(args, struct netif *);
    char *name = va_arg(args, char *);
    *netif_copy = *netif;
    netif_index_to_name(netif_get_index(netif), name);
    return ERR_OK;
}

mp_obj_t netif_inet_ntoa(const ip_addr_t *ipaddr) {
    vstr_t vstr;
    vstr_init(&vstr, IPADDR_STRLEN_MAX);
    const char *s = ipaddr_ntoa_r(ipaddr, vstr.buf, vstr.alloc);
    vstr.len = strnlen(s, vstr.alloc);
    return mp_obj_new_str_from_vstr(&vstr);
}

void netif_inet_aton(mp_obj_t addr_in, ip_addr_t *ipaddr) {
    const char *addr = mp_obj_str_get_str(addr_in);
    if (!ipaddr_aton(addr, ipaddr)) {
        mp_raise_ValueError(NULL);
    }
}

static void netif_inet4_aton(mp_obj_t addr_in, ip4_addr_t *ipaddr) {
    const char *addr = mp_obj_str_get_str(addr_in);
    if (!ip4addr_aton(addr, ipaddr)) {
        mp_raise_ValueError(NULL);
    }
}

static mp_obj_t netif_dict(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    struct netif netif;
    char name[NETIF_NAMESIZE];
    netif_call_raise(self->index, netif_lwip_dict, &netif, name);

    mp_obj_t dict = mp_obj_new_dict(16);
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_index), MP_OBJ_NEW_SMALL_INT(netif_get_index(&netif)));
    #if LWIP_IPV4
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_address), netif_inet_ntoa(netif_ip_addr4(&netif)));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_gateway), netif_inet_ntoa(netif_ip_gw4(&netif)));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_netmask), netif_inet_ntoa(netif_ip_netmask4(&netif)));
    #endif
    #if LWIP_DHCP
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_dhcp), mp_obj_new_bool(dhcp_supplied_address(&netif)));
    #endif
    #if LWIP_IPV6
    mp_obj_t list = mp_obj_new_list(0, NULL);
    for (size_t i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_isvalid(netif_ip6_addr_state(&netif, i))) {
            mp_obj_list_append(list, netif_inet_ntoa(netif_ip_addr6(&netif, i)));
        }
    }
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_addresses), list);
    #endif

    vstr_t hwaddr;
    vstr_init(&hwaddr, ETH_HWADDR_LEN * 3);
    for (size_t i = 0; i < netif.hwaddr_len; i++) {
        vstr_printf(&hwaddr, "%02x:", netif.hwaddr[i]);
    }
    vstr_cut_tail_bytes(&hwaddr, 1);
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_mac), mp_obj_new_str_from_vstr(&hwaddr));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_mtu), MP_OBJ_NEW_SMALL_INT(netif.mtu));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_enabled), mp_obj_new_bool(netif_is_up(&netif)));
    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_link_up), mp_obj_new_bool(netif_is_link_up(&netif)));

    mp_obj_dict_store(dict, MP_OBJ_NEW_QSTR(MP_QSTR_name), mp_obj_new_str(name, strlen(name)));

    return dict;
}
static MP_DEFINE_CONST_FUN_OBJ_1(netif_dict_obj, netif_dict);

static void netif_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
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

static void netif_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    struct netif netif;
    char name[NETIF_NAMESIZE];
    netif_call_raise(self->index, netif_lwip_dict, &netif, name);

    char address[IP4ADDR_STRLEN_MAX];
    ip4addr_ntoa_r(netif_ip4_addr(&netif), address, IP4ADDR_STRLEN_MAX);
    if (netif_is_up(&netif)) {
        mp_printf(print, "NetInterface(name=%s, address=%s, link=%s)", name, address, netif_is_link_up(&netif) ? "up" : "down");
    }
    else {
        mp_printf(print, "NetInterface(name=%s, disabled)", name);
    }
}

static err_t netif_lwip_configure(struct netif *netif, va_list args) {
    ip4_addr_t *address = va_arg(args, ip4_addr_t *);
    ip4_addr_t *netmask = va_arg(args, ip4_addr_t *);
    ip4_addr_t *gateway = va_arg(args, ip4_addr_t *);
    netif_set_addr(netif, address, netmask, gateway);
    #if LWIP_DHCP
    dhcp_inform(netif);
    #endif
    return ERR_OK;
}

static mp_obj_t netif_configure(size_t n_args, const mp_obj_t *args) {
    netif_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    ip4_addr_t address, netmask, gateway;
    netif_inet4_aton(args[1], &address);
    netif_inet4_aton(args[2], &netmask);
    netif_inet4_aton(args[3], &gateway);    

    netif_call_raise(self->index, netif_lwip_configure, &address, &netmask, &gateway);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(netif_configure_obj, 4, 4, netif_configure);

#if LWIP_DHCP
static err_t netif_lwip_dhcp_start(struct netif *netif, va_list args) {
    return dhcp_start(netif);
}

static mp_obj_t netif_dhcp_start(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    netif_call_raise(self->index, netif_lwip_dhcp_start);
    return mp_const_none;

}
static MP_DEFINE_CONST_FUN_OBJ_1(netif_dhcp_start_obj, netif_dhcp_start);

static err_t netif_lwip_dhcp_stop(struct netif *netif, va_list args) {
    dhcp_release_and_stop(netif);
    return ERR_OK;
}

static mp_obj_t netif_dhcp_stop(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    netif_call_raise(self->index, netif_lwip_dhcp_stop);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(netif_dhcp_stop_obj, netif_dhcp_stop);

static err_t netif_lwip_dhcp_renew(struct netif *netif, va_list args) {
    return dhcp_renew(netif);
}

static mp_obj_t netif_dhcp_renew(mp_obj_t self_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    netif_call_raise(self->index, netif_lwip_dhcp_renew);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(netif_dhcp_renew_obj, netif_dhcp_renew);
#endif

static err_t netif_lwip_enable(struct netif *netif, va_list args) {
    int enable = va_arg(args, int);
    if (enable) {
        netif_set_up(netif);
    }
    else {
        #if LWIP_DHCP
        dhcp_release_and_stop(netif);
        #endif
        netif_set_down(netif);
    }
    return ERR_OK;
}

static mp_obj_t netif_enable(mp_obj_t self_in, mp_obj_t enable_in) {
    netif_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int enable = mp_obj_is_true(enable_in);
    netif_call_raise(self->index, netif_lwip_enable, enable);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(netif_enable_obj, netif_enable);

static void netif_lwip_status_callback(struct netif *netif) {
    netif_obj_t *self = netif_get_client_data(netif, netif_lwip_client_id());
    if (self) {
        poll_file_notify(self->poll.file, 0, POLLIN);
    }
    netif_set_status_callback(netif, NULL);
}

static err_t netif_lwip_wait(struct netif *netif, va_list args) {
    netif_obj_t *self = va_arg(args, netif_obj_t *);
    if (ip_addr_isany(netif_ip_addr4(netif)) || !netif_is_link_up(netif)) {
        poll_file_notify(self->poll.file, POLLIN, 0);
        netif_set_status_callback(netif, netif_lwip_status_callback);
        return ERR_WOULDBLOCK;
    }
    return ERR_OK;
}

static mp_obj_t netif_wait(size_t n_args, const mp_obj_t *args) {
    netif_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    int timeout = -1;
    if ((n_args > 1) && (args[1] != mp_const_none)) {
        timeout = mp_obj_get_int(args[1]);
    }

    TickType_t xTicksToWait = (timeout < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
    err_t err;
    do {
        err = netif_call(self->index, netif_lwip_wait, self);
    }
    while ((err == ERR_WOULDBLOCK) && mp_poll_wait(&self->poll, POLLIN, &xTicksToWait));

    socket_lwip_raise(err);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(netif_wait_obj, 1, 2, netif_wait);

static const mp_rom_map_elem_t netif_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&netif_del_obj) },
    { MP_ROM_QSTR(MP_QSTR___dict__),        MP_ROM_PTR(&netif_dict_obj) },
    { MP_ROM_QSTR(MP_QSTR_configure),       MP_ROM_PTR(&netif_configure_obj) },
    #if LWIP_DHCP
    { MP_ROM_QSTR(MP_QSTR_dhcp_start),      MP_ROM_PTR(&netif_dhcp_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_dhcp_stop),       MP_ROM_PTR(&netif_dhcp_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_dhcp_renew),      MP_ROM_PTR(&netif_dhcp_renew_obj) },
    #endif
    { MP_ROM_QSTR(MP_QSTR_enable),          MP_ROM_PTR(&netif_enable_obj) },
    { MP_ROM_QSTR(MP_QSTR_wait),            MP_ROM_PTR(&netif_wait_obj) },
};
static MP_DEFINE_CONST_DICT(netif_locals_dict, netif_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    netif_type,
    MP_QSTR_NetInterface,
    MP_TYPE_FLAG_NONE,
    // make_new, netif_make_new,
    print, netif_print,
    attr, netif_attr,
    locals_dict, &netif_locals_dict
    );


static mp_obj_t netif_list_make_new(const mp_obj_type_t *type, size_t n_args, const mp_obj_t *args) {
    mp_arg_check_num(n_args, 0, 0, 0, false);
    mp_obj_base_t *netif_list = m_new_obj(mp_obj_base_t);
    netif_list->type = type;
    return MP_OBJ_FROM_PTR(netif_list);
}

static err_t netif_lwip_set_default(struct netif *netif, va_list args) {
    netif_set_default(netif);
    return ERR_OK;
}

static void netif_list_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
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

static mp_obj_t netif_list_subscr(mp_obj_t self_in, mp_obj_t index_in, mp_obj_t value) {
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

static mp_obj_t netif_list_tuple(mp_obj_t self_in) {
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

static mp_obj_t netif_list_getiter(mp_obj_t self_in,  mp_obj_iter_buf_t *iter_buf) {
    mp_obj_t tuple = netif_list_tuple(self_in);
    return mp_obj_tuple_getiter(tuple, iter_buf);
}

static mp_obj_t netif_list_unary_op(mp_unary_op_t op, mp_obj_t self_in) {
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

static mp_obj_t netif_dns_servers_get(void) {
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
        items[len++] = netif_inet_ntoa(&dns_servers[i]);
    }
    return mp_obj_new_list(len, items);
}

static mp_obj_t netif_dns_servers_set(mp_obj_t value) {
    size_t len = 0;
    mp_obj_t *items;
    mp_obj_list_get(value, &len, &items);
    if (len > DNS_MAX_SERVERS) {
        mp_raise_ValueError(NULL);
    }
    
    ip_addr_t dns_servers[DNS_MAX_SERVERS];
    for (size_t i = 0; i < len; i++) {
        netif_inet_aton(items[i], &dns_servers[i]);
    }
    
    LOCK_TCPIP_CORE();
    for (size_t i = 0; i < DNS_MAX_SERVERS; i++) {
        dns_setserver(i, (i < len) ? &dns_servers[i] : NULL);
    }
    UNLOCK_TCPIP_CORE();
    return mp_const_none;
}

static mp_obj_t netif_dns_servers(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return netif_dns_servers_get();
    }
    else {
        return netif_dns_servers_set(args[0]);
    }
}
MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(netif_dns_servers_obj, 0, 1, netif_dns_servers);

static mp_obj_t netif_getattr(mp_obj_t attr) {
    switch (MP_OBJ_QSTR_VALUE(attr)) {
        case MP_QSTR_netif:
            return netif_list_make_new(&netif_list_type, 0, NULL);
        default:
            return MP_OBJ_NULL;
    }
}
MP_DEFINE_CONST_FUN_OBJ_1(netif_getattr_obj, netif_getattr);
