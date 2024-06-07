// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <string.h>

#include "./dns.h"
#include "./netif.h"
#include "./socket.h"
#include "./tcp.h"
#include "./udp.h"
#include "shared/netutils/netutils.h"
#include "py/mpconfig.h"


#define AF_INET         2

#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

static mp_obj_t socket_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 3, false);
    mp_int_t family = AF_INET;
    mp_int_t sock_type = SOCK_STREAM;
    mp_int_t proto = 0;
    if(n_args > 0) {
        family = mp_obj_get_int(args[0]);
    }
    if(n_args > 1) {
        sock_type = mp_obj_get_int(args[1]);
    }
    if(n_args > 2) {
        proto = mp_obj_get_int(args[2]);
    }
    if (family != AF_INET) {
        mp_raise_ValueError(NULL);
    }

    const struct socket_vtable *vtable;
    if (sock_type == SOCK_STREAM && proto == 0) {
        vtable = &socket_tcp_vtable;
    }
    else if (sock_type == SOCK_DGRAM && proto == 0) {
        vtable = &socket_udp_vtable;
    }    
    else {
        mp_raise_ValueError(NULL);
    }

    socket_obj_t *self = socket_new(type, vtable);
    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_new ? self->func->lwip_new(self) : ERR_VAL;
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);
    return MP_OBJ_FROM_PTR(self);
}

static void socket_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "<socket family=%d type=%d proto=%d>", AF_INET, self->func == &socket_udp_vtable ? SOCK_DGRAM : SOCK_STREAM, 0);
}

static mp_uint_t socket_close(mp_obj_t self_in, int *errcode) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);   
    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_close ? self->func->lwip_close(self) : ERR_VAL;
    UNLOCK_TCPIP_CORE();    
    if (socket_lwip_err(err, errcode)) {
        return MP_STREAM_ERROR;
    }

    self->user_closed = 1;
    if (!self->errcode) {
        self->errcode = MP_EBADF;
    }
    mp_stream_poll_close(&self->poll);
    socket_call_cleanup(self);
    return 0;
}

static mp_obj_t socket_del(mp_obj_t self_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    LOCK_TCPIP_CORE();
    if (self->func->lwip_abort) {
        self->func->lwip_abort(self);
    }
    UNLOCK_TCPIP_CORE();
    socket_call_cleanup(self);
    socket_deinit(self);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_del_obj, socket_del);

static mp_obj_t socket_getpeername(mp_obj_t self_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    socket_acquire(self);
    int errcode = self->errcode;
    int connected = self->connected;
    struct sockaddr address = self->remote;
    socket_release(self);
    if (errcode) {
        mp_raise_OSError(errcode);
    }
    if (!connected) {
        mp_raise_OSError(MP_ENOTCONN);
    }
    return socket_sockaddr_format(&address);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_getpeername_obj, socket_getpeername);

static mp_obj_t socket_getsockname(mp_obj_t self_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    socket_acquire(self);
    int errcode = self->errcode;
    struct sockaddr address = self->local;
    socket_release(self);
    if (errcode) {
        mp_raise_OSError(errcode);
    }    
    return socket_sockaddr_format(&address);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_getsockname_obj, socket_getsockname);

static mp_obj_t socket_check_ret(mp_uint_t ret, int errcode) {
    return mp_stream_return(ret, errcode);
}

static mp_obj_t socket_bind(mp_obj_t self_in, mp_obj_t address_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    struct sockaddr address;
    sokcet_sockaddr_parse(address_in, &address);
    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_bind ? self->func->lwip_bind(self, &address) : ERR_VAL;
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_bind_obj, socket_bind);

static mp_obj_t socket_listen(size_t n_args, const mp_obj_t *args) {
    socket_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_int_t backlog = TCP_DEFAULT_LISTEN_BACKLOG;
    if (n_args > 1) {
        backlog = mp_obj_get_int(args[1]);
    }
    backlog = MAX(0, backlog);

    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_listen ? self->func->lwip_listen(self, backlog) : ERR_VAL;
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);

    self->listening = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_listen_obj, 1, 2, socket_listen);

static mp_obj_t socket_accept(mp_obj_t self_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);

    socket_obj_t *new_self;
    int errcode;
    mp_uint_t ret = self->func->socket_accept(self, &new_self, &errcode);
    mp_obj_t result = socket_check_ret(ret, errcode);

    if (result != mp_const_none) {
        mp_obj_t items[2] = {
            MP_OBJ_FROM_PTR(new_self),
            socket_sockaddr_format(&new_self->remote),
        };
        result = mp_obj_new_tuple(2, items);
    }
    return result;
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_accept_obj, socket_accept);

static mp_uint_t socket_connected(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_uint_t ret = 0;
    socket_acquire(self);
    if (self->errcode) {
        *errcode = self->errcode;
        ret = MP_STREAM_ERROR;
    }
    else if (!self->connected) {
        *errcode = self->connecting ? MP_EAGAIN : MP_ENOTCONN;
        ret = MP_STREAM_ERROR;
    }
    socket_release(self);
    return ret;
}

static mp_obj_t socket_connect(mp_obj_t self_in, mp_obj_t address_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    struct sockaddr address;
    sokcet_sockaddr_parse(address_in, &address);
    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_connect ? self->func->lwip_connect(self, &address) : ERR_VAL;
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);
    self->connecting = 1;
    
    int errcode;
    mp_uint_t ret = mp_poll_block(self_in, NULL, 0, &errcode, socket_connected, MP_STREAM_POLL_RD | MP_STREAM_POLL_WR, self->timeout, false);
    return socket_check_ret(ret, errcode);
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_obj, socket_connect);

static mp_obj_t socket_isconnected(mp_obj_t self_in) {
    int errcode;
    mp_uint_t ret = socket_connected(self_in, NULL, 0, &errcode);
    mp_obj_t result = socket_check_ret(ret, errcode);
    return (result == mp_const_none) ? mp_const_false : mp_const_true;
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_isconnected_obj, socket_isconnected);

static mp_obj_t socket_recvfrom_interal(mp_obj_t self_in, mp_obj_t bufsize_in, struct sockaddr *address) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_int_t bufsize = mp_obj_get_int(bufsize_in);
    if (bufsize < 0) {
        mp_raise_ValueError(NULL);
    }

    vstr_t buf;
    vstr_init_len(&buf, bufsize);
    int errcode;
    mp_uint_t ret = self->func->socket_recvfrom(self, buf.buf, buf.alloc, address, &errcode);
    mp_obj_t result = socket_check_ret(ret, errcode);

    if (result != mp_const_none) {
        buf.len = MP_OBJ_SMALL_INT_VALUE(result);
        result = mp_obj_new_bytes_from_vstr(&buf);
    }
    return result;
}

static mp_obj_t socket_recv(mp_obj_t self_in, mp_obj_t bufsize_in) {
    return socket_recvfrom_interal(self_in, bufsize_in, NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_recv_obj, socket_recv);

static mp_obj_t socket_recvfrom(mp_obj_t self_in, mp_obj_t bufsize_in) {
    struct sockaddr address;
    mp_obj_t result = socket_recvfrom_interal(self_in, bufsize_in, &address);

    if (result != mp_const_none) {
        mp_obj_t items[] = {
            result,
            socket_sockaddr_format(&address),
        };
        result = mp_obj_new_tuple(2, items);
    }
    return result;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_recvfrom_obj, socket_recvfrom);

static mp_obj_t socket_recvfrom_into_internal(size_t n_args, const mp_obj_t *args, struct sockaddr *address) {
    socket_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_WRITE);
    mp_int_t nbytes = bufinfo.len;
    if (n_args > 2) {
        nbytes = mp_obj_get_int(args[2]);
    }
    if (nbytes < 0) {
        mp_raise_ValueError(NULL);
    }

    int errcode;
    mp_uint_t ret = self->func->socket_recvfrom(self, bufinfo.buf, bufinfo.len, address, &errcode);
    return socket_check_ret(ret, errcode);
}

static mp_obj_t socket_recv_into(size_t n_args, const mp_obj_t *args) {
    return socket_recvfrom_into_internal(n_args, args, NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recv_into_obj, 2, 3, socket_recv_into);

static mp_obj_t socket_recvfrom_into(size_t n_args, const mp_obj_t *args) {
    struct sockaddr address;
    mp_obj_t result = socket_recvfrom_into_internal(n_args, args, &address);

    if (result != mp_const_none) {
        mp_obj_t items[] = {
            result,
            socket_sockaddr_format(&address),
        };
        result = mp_obj_new_tuple(2, items);
    }
    return result;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_recvfrom_into_obj, 2, 3, socket_recvfrom_into);

static mp_uint_t socket_stream_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return self->func->socket_recvfrom(self, buf, size, NULL, errcode);
}

static size_t socket_find_newline(socket_obj_t *self) {
    if (!self->rx_data) {
        return -1u;
    }
    uint8_t nl = '\n';
    u16_t pos = pbuf_memfind(self->rx_data, &nl, sizeof(uint8_t), self->rx_offset);
    if (pos == -1u) {
        return -1u;
    }
    pos -= self->rx_offset;
    if (pos >= self->rx_len) {
        return -1u;
    }
    return pos;
}

static mp_obj_t socket_readline(size_t n_args, const mp_obj_t *args) {
    socket_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    size_t size = -1u;
    if (n_args > 1) {
        size = mp_obj_get_int(args[1]);
    }

    vstr_t vstr;
    vstr_init(&vstr, 0);
    bool found = false;
    while (!found) {
        socket_acquire(self);
        size_t len = self->rx_len + 1;
        if (self->rx_len >= size) {
            found = true;
            len = size;
        }
        size_t pos = socket_find_newline(self);
        if (pos != -1u) {
            found = true;
            len = pos + 1;
        }
        socket_release(self);

        char *buf = vstr_add_len(&vstr, len);
        int errcode;
        mp_uint_t ret = self->func->socket_recvfrom(self, buf, len, NULL, &errcode);
        socket_check_ret(ret, errcode);
        assert(ret == len);
        if (size != -1u) {
            size -= ret;
        }
    }
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_readline_obj, 1, 2, socket_readline);

static mp_uint_t socket_sendto_nonblock(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    socket_sendto_args_t *args = buf;
    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_sendto ? self->func->lwip_sendto(self, args) : ERR_VAL;
    UNLOCK_TCPIP_CORE();
    if (socket_lwip_err(err, errcode)) {
        return MP_STREAM_ERROR;    
    }
    return args->len;
}

static mp_uint_t socket_sendto_block(mp_obj_t self_in, socket_sendto_args_t *args, int *errcode) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return mp_poll_block(self_in, args, sizeof(socket_sendto_args_t), errcode, socket_sendto_nonblock, MP_STREAM_POLL_WR, self->timeout, false);
}

static mp_obj_t socket_sendto(mp_obj_t self_in, mp_obj_t bytes_in, mp_obj_t address_in) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(bytes_in, &bufinfo, MP_BUFFER_READ);

    socket_sendto_args_t args;
    struct sockaddr address;    
    args.buf = bufinfo.buf;
    args.len = bufinfo.len;
    args.address = NULL;
    if (address_in != MP_OBJ_NULL) {    
        sokcet_sockaddr_parse(address_in, &address);
        args.address = &address;
    }
    int errcode;
    mp_uint_t ret = socket_sendto_block(self_in, &args, &errcode);
    return socket_check_ret(ret, errcode);
}
static MP_DEFINE_CONST_FUN_OBJ_3(socket_sendto_obj, socket_sendto);

static mp_obj_t socket_send(mp_obj_t self_in, mp_obj_t bytes_in) {
    return socket_sendto(self_in, bytes_in, MP_OBJ_NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_send_obj, socket_send);

static mp_obj_t socket_sendall(mp_obj_t self_in, mp_obj_t bytes_in) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(bytes_in, &bufinfo, MP_BUFFER_READ);
    socket_sendto_args_t args;
    args.buf = bufinfo.buf;
    args.len = bufinfo.len;
    args.address = NULL;

    while (args.len > 0) {
        int errcode;
        mp_uint_t ret = socket_sendto_block(self_in, &args, &errcode);
        if (ret == MP_STREAM_ERROR) {
            mp_raise_OSError(errcode);
        }
        args.buf += ret;
        args.len -= ret;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_sendall_obj, socket_sendall);

static mp_uint_t socket_stream_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    socket_sendto_args_t args;
    args.buf = buf;
    args.len = size;
    args.address = NULL;
    return socket_sendto_block(self_in, &args, errcode);
}

static mp_obj_t socket_shutdown(mp_obj_t self_in, mp_obj_t how_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_uint_t how = mp_obj_get_int(how_in);
    bool shut_rx = (how == SHUT_RD) || (how == SHUT_RDWR);
    bool shut_tx = (how == SHUT_WR) || (how == SHUT_RDWR);

    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_shutdown ? self->func->lwip_shutdown(self, shut_rx, shut_tx) : ERR_VAL;
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);

    if (shut_tx) {

    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(socket_shutdown_obj, socket_shutdown);

static mp_uint_t socket_flush(mp_obj_t self_in, int *errcode) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_output ? self->func->lwip_output(self) : ERR_VAL;
    UNLOCK_TCPIP_CORE();
    if (socket_lwip_err(err, errcode)) {
        return MP_STREAM_ERROR;
    }
    return 0;
}

static mp_uint_t socket_timeout(mp_obj_t self_in, mp_int_t timeout, int *errcode) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->user_closed) {
        *errcode = self->errcode;
        return MP_STREAM_ERROR;
    }
    self->timeout = timeout >= 0 ? pdMS_TO_TICKS(timeout) : portMAX_DELAY;
    return 0;
}

static mp_obj_t socket_getsockopt(size_t n_args, const mp_obj_t *args) {
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_getsockopt_obj, 3, 3, socket_getsockopt);

static mp_obj_t socket_setsockopt(size_t n_args, const mp_obj_t *args) {
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_setsockopt_obj, 3, 4, socket_setsockopt);

static mp_uint_t socket_ioctl(mp_obj_t self_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);

    uint32_t ret;
    switch (request) {
        case MP_STREAM_FLUSH:
            ret = socket_flush(self_in, errcode);
            break;
        case MP_STREAM_TIMEOUT:
            ret = socket_timeout(self_in, arg, errcode);
            break;
        case MP_STREAM_POLL_CTL:
            socket_acquire(self);
            ret = mp_stream_poll_ctl(&self->poll, (void*)arg, errcode);
            socket_release(self);
            break;
        case MP_STREAM_CLOSE:
            ret = socket_close(self_in, errcode);
            break;
        default:
            *errcode = MP_EINVAL;
            ret = MP_STREAM_ERROR;
            break;
    }
    return ret;
}

static mp_obj_t socket_available(mp_obj_t self_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    return MP_OBJ_NEW_SMALL_INT(self->rx_len);
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_available_obj, socket_available);

static const mp_rom_map_elem_t socket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&socket_del_obj) },
    { MP_ROM_QSTR(MP_QSTR_bind),            MP_ROM_PTR(&socket_bind_obj) },
    { MP_ROM_QSTR(MP_QSTR_listen),          MP_ROM_PTR(&socket_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_accept),          MP_ROM_PTR(&socket_accept_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect),         MP_ROM_PTR(&socket_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv),            MP_ROM_PTR(&socket_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom),        MP_ROM_PTR(&socket_recvfrom_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv_into),       MP_ROM_PTR(&socket_recv_into_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom_into),   MP_ROM_PTR(&socket_recvfrom_into_obj) },
    { MP_ROM_QSTR(MP_QSTR_send),            MP_ROM_PTR(&socket_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendall),         MP_ROM_PTR(&socket_sendall_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendto),          MP_ROM_PTR(&socket_sendto_obj) },
    { MP_ROM_QSTR(MP_QSTR_shutdown),        MP_ROM_PTR(&socket_shutdown_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpeername),     MP_ROM_PTR(&socket_getpeername_obj) },
    { MP_ROM_QSTR(MP_QSTR_getsockname),     MP_ROM_PTR(&socket_getsockname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getsockopt),      MP_ROM_PTR(&socket_getsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_setsockopt),      MP_ROM_PTR(&socket_setsockopt_obj) },

    { MP_ROM_QSTR(MP_QSTR_read),            MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_read1),           MP_ROM_PTR(&mp_stream_read1_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto),        MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline),        MP_ROM_PTR(&socket_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write),           MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_write1),          MP_ROM_PTR(&mp_stream_write1_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_stream_close_obj) },    
    { MP_ROM_QSTR(MP_QSTR_settimeout),      MP_ROM_PTR(&mp_stream_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking),     MP_ROM_PTR(&mp_stream_setblocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_flush),           MP_ROM_PTR(&mp_stream_flush_obj) },

    { MP_ROM_QSTR(MP_QSTR_isconnected),     MP_ROM_PTR(&socket_isconnected_obj) },
    { MP_ROM_QSTR(MP_QSTR_available),       MP_ROM_PTR(&socket_available_obj) },
};
static MP_DEFINE_CONST_DICT(socket_locals_dict, socket_locals_dict_table);

static const mp_stream_p_t socket_stream_p = {
    .read = socket_stream_read,
    .write = socket_stream_write,
    .ioctl = socket_ioctl,
    .is_text = 0,
    .can_poll = 1,
};

MP_DEFINE_CONST_OBJ_TYPE(
    socket_type,
    MP_QSTR_Socket,
    MP_TYPE_FLAG_ITER_IS_STREAM,
    make_new, socket_make_new,
    print, socket_print,
    protocol, &socket_stream_p,
    locals_dict, &socket_locals_dict
    );

// ###

static mp_obj_t socket_dns_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    socket_obj_t *self = socket_new(type, &socket_dns_vtable);
    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_new(self);
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t socket_dns_gethostbyname(mp_obj_t self_in, mp_obj_t name_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    socket_sendto_args_t args;
    const char* name = mp_obj_str_get_str(name_in);
    args.buf = name;
    args.len = strlen(name);
    args.address = NULL;
    LOCK_TCPIP_CORE();
    err_t err = self->func->lwip_sendto(self, &args);
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);    
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(dns_socket_gethostbyname_obj, socket_dns_gethostbyname);

static mp_obj_t socket_dns_get(mp_obj_t self_in) {
    socket_obj_t *self = MP_OBJ_TO_PTR(self_in);
    struct sockaddr address;
    vstr_t buf;
    vstr_init_len(&buf, 255);
    int errcode;
    mp_uint_t ret = self->func->socket_recvfrom(self, buf.buf, buf.alloc, &address, &errcode);
    mp_obj_t result = socket_check_ret(ret, errcode);
    if (result == mp_const_none) {
        return mp_const_none;
    }

    buf.len = MP_OBJ_SMALL_INT_VALUE(result);
    result = mp_obj_new_str_from_vstr(&buf);

    mp_obj_t items[] = {
        result,
        ip_addr_isany_val(address.addr) ?  mp_const_none : socket_sockaddr_format(&address),
    };
    return mp_obj_new_tuple(2, items);
}
static MP_DEFINE_CONST_FUN_OBJ_1(dns_socket_get_obj, socket_dns_get);

static const mp_rom_map_elem_t socket_dns_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_gethostbyname),   MP_ROM_PTR(&dns_socket_gethostbyname_obj) },

    { MP_ROM_QSTR(MP_QSTR_settimeout),      MP_ROM_PTR(&mp_stream_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_get),             MP_ROM_PTR(&dns_socket_get_obj) },
};
static MP_DEFINE_CONST_DICT(socket_dns_locals_dict, socket_dns_locals_dict_table);

static const mp_stream_p_t socket_dns_stream_p = {
    .read = NULL,
    .write = NULL,
    .ioctl = socket_ioctl,
    .is_text = 0,
    .can_poll = 1,
};

MP_DEFINE_CONST_OBJ_TYPE(
    socket_dns_type,
    MP_QSTR_DnsSocket,
    MP_TYPE_FLAG_NONE,
    make_new, socket_dns_make_new,
    print, socket_print,    
    protocol, &socket_dns_stream_p,
    locals_dict, &socket_dns_locals_dict
    );

static mp_obj_t socket_gethostbyname(mp_obj_t name) {
    mp_obj_t dns_socket = socket_dns_make_new(&socket_dns_type, 0, 0, NULL);
    socket_dns_gethostbyname(dns_socket, name);
    mp_obj_t result = socket_dns_get(dns_socket);
    int errcode;
    mp_uint_t ret = socket_close(dns_socket, &errcode);
    if (result == mp_const_none) {
        mp_raise_OSError(MP_EINPROGRESS);
    }
    socket_check_ret(ret, errcode);

    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(result, &len, &items);
    if (items[1] == mp_const_none) {
        mp_raise_OSError(MP_ENOENT);
    }

    mp_obj_tuple_get(items[1], &len, &items);
    return items[0];
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_gethostbyname_obj, socket_gethostbyname);

static mp_obj_t socket_getaddrinfo(size_t n_args, const mp_obj_t *args) {
    // mp_arg_check_num(n_args, 0, 2, 6, false);
    mp_obj_t host = args[0];
    mp_obj_get_int(args[1]);
    mp_obj_t address = MP_OBJ_NULL;

    mp_int_t family = 0;
    mp_int_t type = 0;
    mp_int_t proto = 0;
    mp_int_t flags = 0;
    // if constraints were passed then check they are compatible with the supported params
    if (n_args > 2) {
        family = mp_obj_get_int(args[2]);
    }
    if (n_args > 3) {
        type = mp_obj_get_int(args[3]);
    }
    if (n_args > 4) {
        proto = mp_obj_get_int(args[4]);
    }
    if (n_args > 5) {
        flags = mp_obj_get_int(args[5]);
    }
    if (family == 0) {
        family = AF_INET;
    }
    if (type == 0) {
        type = SOCK_STREAM;
    }
    if (!(family == AF_INET
            && (type == SOCK_STREAM || type == SOCK_DGRAM)
            && proto == 0
            && flags == 0)) {
        mp_raise_ValueError(MP_ERROR_TEXT("unsupported getaddrinfo constraints"));
    }

    // check if host is already in IP form
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        ip4_addr_t ipaddr;
        netutils_parse_ipv4_addr(host, (uint8_t*)&ipaddr, NETUTILS_BIG);
        address = netutils_format_ipv4_addr((uint8_t*)&ipaddr, NETUTILS_BIG);
        nlr_pop();
    } else {
        // swallow exception: host was not in IP form so need to do DNS lookup
    }

    if (address == MP_OBJ_NULL) {
        address = socket_gethostbyname(host);
    }

    mp_obj_t items1[] = { address, args[1] };
    mp_obj_t items[] = {
        MP_OBJ_NEW_SMALL_INT(family),
        MP_OBJ_NEW_SMALL_INT(type),
        MP_OBJ_NEW_SMALL_INT(proto),
        MP_OBJ_NEW_QSTR(MP_QSTR_),
        mp_obj_new_tuple(2, items1),
    };
    mp_obj_t tuple = mp_obj_new_tuple(5, items);
    return mp_obj_new_list(1, &tuple);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_getaddrinfo_obj, 2, 6, socket_getaddrinfo);

static mp_obj_t socket_create_connection(size_t n_args, const mp_obj_t *args) {
    if (!mp_obj_is_type(args[0], &mp_type_tuple)) {
        mp_raise_TypeError(NULL);
    }

    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(args[0], &len, &items);
    if (len != 2) {
        mp_raise_TypeError(NULL);
    }

    mp_obj_t items2[] = { socket_gethostbyname(items[0]), items[1] };
    mp_obj_t tuple = mp_obj_new_tuple(2, items2);

    mp_obj_t socket = socket_make_new(&socket_type, 0, 0, NULL);
    socket_connect(socket, tuple);
    return socket;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_create_connection_obj, 1, 3, socket_create_connection);

static mp_obj_t socket_create_server(mp_obj_t address_in) {
    if (!mp_obj_is_type(address_in, &mp_type_tuple)) {
        mp_raise_TypeError(NULL);
    }

    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(address_in, &len, &items);
    if (len != 2) {
        mp_raise_TypeError(NULL);
    }

    mp_obj_t socket = socket_make_new(&socket_type, 0, 0, NULL);
    socket_bind(MP_OBJ_TO_PTR(socket), address_in);
    socket_listen(1, &socket);
    return socket;
}
static MP_DEFINE_CONST_FUN_OBJ_1(socket_create_server_obj, socket_create_server);

static const mp_rom_map_elem_t socket_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_socket) },
    { MP_ROM_QSTR(MP_QSTR_gethostbyname),   MP_ROM_PTR(&socket_gethostbyname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getaddrinfo),     MP_ROM_PTR(&socket_getaddrinfo_obj) },

    { MP_ROM_QSTR(MP_QSTR_socket),          MP_ROM_PTR(&socket_type) },
    { MP_ROM_QSTR(MP_QSTR_DnsSocket),       MP_ROM_PTR(&socket_dns_type) },

    { MP_ROM_QSTR(MP_QSTR_create_connection),   MP_ROM_PTR(&socket_create_connection_obj) },
    { MP_ROM_QSTR(MP_QSTR_create_server),   MP_ROM_PTR(&socket_create_server_obj) },

    { MP_ROM_QSTR(MP_QSTR_NetInterface),    MP_ROM_PTR(&netif_type) },
    { MP_ROM_QSTR(MP_QSTR___getattr__),     MP_ROM_PTR(&netif_getattr_obj) },

    { MP_ROM_QSTR(MP_QSTR_AF_INET),         MP_ROM_INT(AF_INET) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_STREAM),     MP_ROM_INT(SOCK_STREAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_DGRAM),      MP_ROM_INT(SOCK_DGRAM) },
};
static MP_DEFINE_CONST_DICT(socket_module_globals, socket_module_globals_table);

const mp_obj_module_t socket_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&socket_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR_socket, socket_module);
