// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#include "extmod/modos_newlib.h"
#include "extmod/io/modio.h"
#include "extmod/socket/socket.h"
#include "py/parseargs.h"
#include "py/runtime.h"
#include "py/stream.h"


MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_gaierror,
    MP_QSTR_gaierror,
    MP_TYPE_FLAG_NONE,
    make_new, mp_obj_exception_make_new,
    print, mp_obj_exception_print,
    attr, mp_obj_exception_attr,    
    parent, &mp_type_OSError
    );

struct mp_socket_addrinfo_node {
    nlr_jump_callback_node_t nlr;
    struct addrinfo *ai;
};

static void mp_socket_addrinfo_cb(void *ctx) {
    struct mp_socket_addrinfo_node *node = ctx;
    freeaddrinfo(node->ai);
}

static void mp_socket_call_getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct mp_socket_addrinfo_node *node) {
    int ret;
    MP_OS_CALL(ret, getaddrinfo, nodename, servname, hints, &node->ai);
    if (ret >= 0) {
        nlr_push_jump_callback(&node->nlr, mp_socket_addrinfo_cb);
        return;
    }
    if (ret == EAI_SYSTEM) {
        mp_raise_OSError(errno);
    }
    const char *strerror = gai_strerror(ret);
    mp_obj_t args[] = {
        MP_OBJ_NEW_SMALL_INT(ret),
        mp_obj_new_str(strerror, strlen(strerror)),
    };
    mp_obj_t exc = mp_obj_exception_make_new(&mp_type_gaierror, 2, 0, args);
    nlr_raise(exc);
}

static mp_obj_t mp_socket_getaddrinfo(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_host, MP_QSTR_port, MP_QSTR_family, MP_QSTR_type, MP_QSTR_proto, MP_QSTR_flags, 0 };
    const char *host;
    mp_obj_t port_obj;
    int family=AF_UNSPEC, type=0, proto=0, flags=0;
    parse_args_and_kw_map(n_args, args, kw_args, "zO|iiii", kws, &host, &port_obj, &family, &type, &proto, &flags);

    const char *port = NULL;
    char port_val[12];
    if (mp_obj_is_int(port_obj)) {
        snprintf(port_val, 6, "%u", mp_obj_get_int(port_obj));
        port = port_val;
    }
    else if (port_obj != mp_const_none) {
        port = mp_obj_str_get_str(port_obj);
    }
    struct addrinfo hints = { flags, family, type, proto, 0 };
    struct mp_socket_addrinfo_node node;
    mp_socket_call_getaddrinfo(host, port, &hints, &node);

    mp_obj_t list = mp_obj_new_list(0, NULL);
    struct addrinfo *ai = node.ai;
    while (ai) {
        const char *cname = ai->ai_canonname;
        mp_obj_t items[] = {
            MP_OBJ_NEW_SMALL_INT(ai->ai_family),
            MP_OBJ_NEW_SMALL_INT(ai->ai_socktype),
            MP_OBJ_NEW_SMALL_INT(ai->ai_protocol),
            cname ? mp_obj_new_str(cname, strlen(cname)) : mp_const_none,
            mp_socket_sockaddr_format(ai->ai_addr, ai->ai_addrlen),
        };
        mp_obj_t tuple = mp_obj_new_tuple(5, items);
        mp_obj_list_append(list, tuple);
        ai = ai->ai_next;
    }
    nlr_pop_jump_callback(true);
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_socket_getaddrinfo_obj, 2, mp_socket_getaddrinfo);

static mp_obj_t mp_socket_gethostbyname(mp_obj_t hostname_in) {
    const char *hostname = mp_obj_str_get_str(hostname_in);

    struct addrinfo hints = { .ai_family = AF_INET };
    struct mp_socket_addrinfo_node node;
    mp_socket_call_getaddrinfo(hostname, NULL, &hints, &node);

    struct addrinfo *ai = node.ai;
    mp_obj_t result = mp_socket_sockaddr_format(ai->ai_addr, ai->ai_addrlen);
    nlr_pop_jump_callback(true);
    
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(result, &len, &items);
    return items[0];
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_gethostbyname_obj, mp_socket_gethostbyname);

static mp_obj_t mp_socket_gethostname(void) {
    vstr_t vstr;
    vstr_init(&vstr, 256);
    int ret = gethostname(vstr.buf, vstr.alloc);
    mp_os_check_ret(ret);
    vstr.len = strnlen(vstr.buf, vstr.alloc);
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_socket_gethostname_obj, mp_socket_gethostname);

static mp_obj_t mp_socket_ntohl(mp_obj_t x_in) {
    uint32_t x = mp_obj_get_int(x_in);
    return mp_obj_new_int(ntohl(x));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_ntohl_obj, mp_socket_ntohl);

static mp_obj_t mp_socket_ntohs(mp_obj_t x_in) {
    uint16_t x = mp_obj_get_int(x_in);
    return MP_OBJ_NEW_SMALL_INT(ntohs(x));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_ntohs_obj, mp_socket_ntohs);

static mp_obj_t mp_socket_htonl(mp_obj_t x_in) {
    uint32_t x = mp_obj_get_int(x_in);
    return mp_obj_new_int(htonl(x));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_htonl_obj, mp_socket_htonl);

static mp_obj_t mp_socket_htons(mp_obj_t x_in) {
    uint16_t x = mp_obj_get_int(x_in);
    return MP_OBJ_NEW_SMALL_INT(htons(x));
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_htons_obj, mp_socket_htons);

static int mp_socket_af_addr_size(int af) {
    switch (af) {
        case AF_INET:
            return sizeof(struct in_addr);
        case AF_INET6:
            return sizeof(struct in6_addr);
        default:
            mp_raise_OSError(EAFNOSUPPORT);
    }
}

static int mp_socket_af_str_size(int af) {
    switch (af) {
        case AF_INET:
            return INET_ADDRSTRLEN;
        case AF_INET6:
            return INET6_ADDRSTRLEN;
        default:
            mp_raise_OSError(EAFNOSUPPORT);
    }
}

static mp_obj_t mp_socket_inet_pton(mp_obj_t af_in, mp_obj_t ip_string_in) {
    int af = mp_obj_get_int(af_in);
    const char *ip_string = mp_obj_str_get_str(ip_string_in);
    vstr_t vstr;
    vstr_init_len(&vstr, mp_socket_af_addr_size(af));
    int ret = inet_pton(af, ip_string, vstr.buf);
    if (ret <= 0) {
        mp_raise_OSError((ret < 0) ? errno : EINVAL);
    }
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_inet_pton_obj, mp_socket_inet_pton);

static mp_obj_t mp_socket_inet_ntop(mp_obj_t af_in, mp_obj_t packed_ip_in) {
    int af = mp_obj_get_int(af_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(packed_ip_in, &bufinfo, MP_BUFFER_READ);
    if (bufinfo.len != mp_socket_af_addr_size(af)) {
        mp_raise_ValueError(NULL);
    }
    vstr_t vstr;
    vstr_init(&vstr, mp_socket_af_str_size(af));
    const char *s = inet_ntop(af, bufinfo.buf, vstr.buf, vstr.alloc);
    vstr.len = strnlen(s, vstr.alloc);
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_inet_ntop_obj, mp_socket_inet_ntop);

static mp_obj_t mp_socket_inet_aton(mp_obj_t ip_string) {
    return mp_socket_inet_pton(MP_OBJ_NEW_SMALL_INT(AF_INET), ip_string);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_inet_aton_obj, mp_socket_inet_aton);

static mp_obj_t mp_socket_inet_ntoa(mp_obj_t packed_ip) {
    return mp_socket_inet_ntop(MP_OBJ_NEW_SMALL_INT(AF_INET), packed_ip);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_inet_ntoa_obj, mp_socket_inet_ntoa);

static mp_obj_t mp_socket_sethostname(mp_obj_t name_in) {
    const char *name = mp_obj_str_get_str(name_in);
    int ret = setenv("HOSTNAME", name, 1);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_sethostname_obj, mp_socket_sethostname);

static mp_obj_t mp_socket_if_nametoindex(mp_obj_t name_in) {
    const char *name = mp_obj_str_get_str(name_in);
    unsigned index = if_nametoindex(name);
    if (index == 0) {
        mp_raise_OSError(ENXIO);
    }
    return MP_OBJ_NEW_SMALL_INT(index);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_if_nametoindex_obj, mp_socket_if_nametoindex);

static mp_obj_t mp_socket_if_indextoname(mp_obj_t index_in) {
    unsigned index = mp_obj_get_int(index_in);
    vstr_t vstr;
    vstr_init(&vstr, IF_NAMESIZE);
    char *ret = if_indextoname(index, vstr.buf);
    if (!ret) {
        mp_raise_OSError(errno);
    }
    vstr.len = strnlen(vstr.buf, IF_NAMESIZE);
    return mp_obj_new_str_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_if_indextoname_obj, mp_socket_if_indextoname);


static mp_obj_t mp_socket_create_connection(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    const qstr kws[] = { MP_QSTR_address, MP_QSTR_timeout, MP_QSTR_source_address, 0 };
    mp_obj_t gai_args[2];
    mp_obj_t timeout = mp_const_none;
    parse_args_and_kw_map(n_args, args, kw_args, "(OO)|OO", kws, &gai_args[0], &gai_args[1], &timeout, NULL);

    mp_obj_t list = mp_socket_getaddrinfo(2, gai_args, NULL);
    size_t list_len;
    mp_obj_t *list_items;
    mp_obj_list_get(list, &list_len, &list_items);
    int last_error = 0;
    for (size_t i = 0; i < list_len; i++) {
        size_t tuple_len;
        mp_obj_t *tuple_items;
        mp_obj_tuple_get(list_items[i], &tuple_len, &tuple_items);
        assert(tuple_len == 5);

        mp_make_new_fun_t new_sock = MP_OBJ_TYPE_GET_SLOT(&mp_type_socket, make_new);
        mp_obj_t socket = new_sock(&mp_type_socket, 3, 0, tuple_items);

        mp_obj_t sock_args[3];
        if (timeout != mp_const_none) {
            mp_load_method(socket, MP_QSTR_settimeout, sock_args);
            sock_args[2] = timeout;
            mp_call_method_n_kw(1, 0, sock_args);
        }
        
        mp_load_method(socket, MP_QSTR_connect_ex, sock_args);
        sock_args[2] = tuple_items[4];
        mp_obj_t ret = mp_call_method_n_kw(1, 0, sock_args);

        last_error = mp_obj_get_int(ret);
        if (last_error == 0) {
            return socket;
        }

        mp_load_method(socket, MP_QSTR_close, sock_args);
        mp_call_method_n_kw(0, 0, sock_args);
    }
    mp_raise_OSError(last_error);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_socket_create_connection_obj, 2, mp_socket_create_connection);


static const mp_rom_map_elem_t mp_module_socket_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_socket) },

    { MP_ROM_QSTR(MP_QSTR_socket),          MP_ROM_PTR(&mp_type_socket) },
    { MP_ROM_QSTR(MP_QSTR_create_connection),   MP_ROM_PTR(&mp_socket_create_connection_obj) },

    { MP_ROM_QSTR(MP_QSTR_getaddrinfo),     MP_ROM_PTR(&mp_socket_getaddrinfo_obj) },
    { MP_ROM_QSTR(MP_QSTR_gethostbyname),   MP_ROM_PTR(&mp_socket_gethostbyname_obj) },
    { MP_ROM_QSTR(MP_QSTR_gethostname),     MP_ROM_PTR(&mp_socket_gethostname_obj) },
    { MP_ROM_QSTR(MP_QSTR_ntohl),           MP_ROM_PTR(&mp_socket_ntohl_obj) },
    { MP_ROM_QSTR(MP_QSTR_ntohs),           MP_ROM_PTR(&mp_socket_ntohs_obj) },
    { MP_ROM_QSTR(MP_QSTR_htonl),           MP_ROM_PTR(&mp_socket_htonl_obj) },
    { MP_ROM_QSTR(MP_QSTR_htons),           MP_ROM_PTR(&mp_socket_htons_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_aton),       MP_ROM_PTR(&mp_socket_inet_aton_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_ntoa),       MP_ROM_PTR(&mp_socket_inet_ntoa_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_pton),       MP_ROM_PTR(&mp_socket_inet_pton_obj) },
    { MP_ROM_QSTR(MP_QSTR_inet_ntop),       MP_ROM_PTR(&mp_socket_inet_ntop_obj) },
    { MP_ROM_QSTR(MP_QSTR_sethostname),     MP_ROM_PTR(&mp_socket_sethostname_obj) },
    { MP_ROM_QSTR(MP_QSTR_if_nametoindex),  MP_ROM_PTR(&mp_socket_if_nametoindex_obj) },
    { MP_ROM_QSTR(MP_QSTR_if_indextoname),  MP_ROM_PTR(&mp_socket_if_indextoname_obj) },
    

    { MP_ROM_QSTR(MP_QSTR_AF_INET),         MP_ROM_INT(AF_INET) },
    { MP_ROM_QSTR(MP_QSTR_AF_INET6),        MP_ROM_INT(AF_INET6) },
    { MP_ROM_QSTR(MP_QSTR_AF_UNSPEC),       MP_ROM_INT(AF_UNSPEC) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_STREAM),     MP_ROM_INT(SOCK_STREAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_DGRAM),      MP_ROM_INT(SOCK_DGRAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_RAW),        MP_ROM_INT(SOCK_RAW) },
    { MP_ROM_QSTR(MP_QSTR_SOL_SOCKET),      MP_ROM_INT(SOL_SOCKET) },
    { MP_ROM_QSTR(MP_QSTR_SOMAXCONN),       MP_ROM_INT(SOMAXCONN) },
    { MP_ROM_QSTR(MP_QSTR_has_ipv6),        MP_ROM_INT(LWIP_IPV6) },
};
static MP_DEFINE_CONST_DICT(mp_module_socket_globals, mp_module_socket_globals_table);

const mp_obj_module_t mp_module_socket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_socket_globals,
};

MP_REGISTER_MODULE(MP_QSTR_socket, mp_module_socket);
