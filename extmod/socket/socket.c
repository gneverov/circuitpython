// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <memory.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "./socket.h"
#include "extmod/io/modio.h"
#include "extmod/modos_newlib.h"
#include "py/builtin.h"
#include "py/parseargs.h"
#include "py/runtime.h"
#include "py/stream.h"


socklen_t mp_socket_sockaddr_parse(mp_obj_t address_in, struct sockaddr_storage *address) {
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(address_in, &len, &items);
    if (len < 2) {
        goto exit;
    }
    const char *host = mp_obj_str_get_str(items[0]);
    mp_int_t port = mp_obj_get_int(items[1]);

    #if LWIP_IPV4
    if (len == 2) {
        struct sockaddr_in *sa = (struct sockaddr_in *)address;
        sa->sin_family = AF_INET;
        sa->sin_port = htons(port);
        if (inet_pton(AF_INET, host, &sa->sin_addr)) {
            return sizeof(*sa);
        }
    }
    #endif

    #if LWIP_IPV6
    mp_int_t flowinfo = (len > 2) ? mp_obj_get_int(items[2]) : 0;
    mp_int_t scope_id = (len > 3) ? mp_obj_get_int(items[3]) : 0;

    if ((len == 2) || (len == 4)){
        struct sockaddr_in6 *sa = (struct sockaddr_in6 *)address;
        sa->sin6_family = AF_INET6;
        sa->sin6_port = htons(port);
        sa->sin6_flowinfo = flowinfo;
        sa->sin6_scope_id = scope_id;
        if (inet_pton(AF_INET6, host, &sa->sin6_addr)) {
            return sizeof(*sa);
        }
    }
    #endif

exit:
    mp_raise_TypeError(NULL);
}

mp_obj_t mp_socket_sockaddr_format(const struct sockaddr *address, socklen_t address_len) {
    switch (address->sa_family) {
        #if LWIP_IPV4
        case AF_INET: {
            assert(address_len >= sizeof(struct sockaddr_in));
            const struct sockaddr_in *sa = (const struct sockaddr_in *)address;
            vstr_t host;
            vstr_init(&host, INET_ADDRSTRLEN);
            if (!inet_ntop(AF_INET, &sa->sin_addr, host.buf, INET_ADDRSTRLEN)) {
                break;
            }
            host.len = strlen(host.buf);
            mp_obj_t items[2] = {
                mp_obj_new_str_from_vstr(&host),
                MP_OBJ_NEW_SMALL_INT(ntohs(sa->sin_port)),
            };
            return mp_obj_new_tuple(2, items);
        }
        #endif
        #if LWIP_IPV6
        case AF_INET6: {
            assert(address_len >= sizeof(struct sockaddr_in6));
            const struct sockaddr_in6 *sa = (const struct sockaddr_in6 *)address;
            vstr_t host;
            vstr_init(&host, INET6_ADDRSTRLEN);
            if (!inet_ntop(AF_INET6, &sa->sin6_addr, host.buf, INET6_ADDRSTRLEN)) {
                break;
            }
            host.len = strlen(host.buf);
            mp_obj_t items[4] = {
                mp_obj_new_str_from_vstr(&host),
                MP_OBJ_NEW_SMALL_INT(ntohs(sa->sin6_port)),
                MP_OBJ_NEW_SMALL_INT(sa->sin6_flowinfo),
                MP_OBJ_NEW_SMALL_INT(sa->sin6_scope_id),
            };
            return mp_obj_new_tuple(4, items);
        }
        #endif        
        default: {
            errno = EAFNOSUPPORT;
        }
    }
    mp_raise_OSError(errno);
}

static mp_obj_socket_t *mp_socket_get(mp_obj_t self_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->fd == -1) {
        mp_raise_ValueError(MP_ERROR_TEXT("closed socket"));
    }
    return self;
}

static mp_obj_t mp_socket_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    const qstr kws[] = { MP_QSTR_family, MP_QSTR_type, MP_QSTR_proto, MP_QSTR_fileno, 0 };
    mp_int_t family = AF_INET;
    mp_int_t sock_type = SOCK_STREAM;
    mp_int_t proto = 0;
    mp_obj_t fileno = mp_const_none;
    parse_args_and_kw(n_args, n_kw, args, "|iiiO", kws, &family, &sock_type, &proto, &fileno);
    int fd = (fileno == mp_const_none) ? -1 : mp_obj_get_int(fileno);

    mp_obj_socket_t *self = mp_obj_malloc(mp_obj_socket_t, &mp_type_socket);
    self->fd = (fd < 0) ? socket(family, sock_type, proto) : fd;
    mp_os_check_ret(self->fd);
    return MP_OBJ_FROM_PTR(self);
}

static mp_obj_t mp_socket_getsockopt_attr(mp_obj_socket_t *self, int name) {
    int value = 0;
    size_t len = sizeof(value);
    mp_os_check_ret(getsockopt(self->fd, SOL_SOCKET, name, &value, &len));
    return MP_OBJ_NEW_SMALL_INT(value);
}

static void mp_socket_attr(mp_obj_t self_in, qstr attr, mp_obj_t *dest) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    if (dest[0] == MP_OBJ_SENTINEL) {
        return;
    }
    switch (attr) {
        case MP_QSTR_family:
            dest[0] = mp_socket_getsockopt_attr(self, SO_DOMAIN);
            break;
        case MP_QSTR_type:
            dest[0] = mp_socket_getsockopt_attr(self, SO_TYPE);
            break;
        case MP_QSTR_proto:
            dest[0] = mp_socket_getsockopt_attr(self, SO_PROTOCOL);
            break;
        default:
            dest[1] = MP_OBJ_SENTINEL;
            break;
    }
}

static mp_obj_t mp_socket_accept(mp_obj_t self_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);

    int ret;
    MP_OS_CALL(ret, accept, self->fd, NULL, NULL);
    mp_os_check_ret(ret);

    mp_obj_socket_t *new_self = mp_obj_malloc(mp_obj_socket_t, self->base.type);
    new_self->fd = ret;
    return MP_OBJ_FROM_PTR(new_self);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_accept_obj, mp_socket_accept);

static mp_obj_t mp_socket_bind(mp_obj_t self_in, mp_obj_t address_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    struct sockaddr_storage address;
    socklen_t address_len = mp_socket_sockaddr_parse(address_in, &address);
    
    int ret = bind(self->fd, (struct sockaddr *)&address, address_len);
    mp_os_check_ret(ret);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_bind_obj, mp_socket_bind);

static mp_obj_t mp_socket_close(mp_obj_t self_in) {
    mp_obj_socket_t *self = MP_OBJ_TO_PTR(self_in);
    if (self->fd >= 0) {
        close(self->fd);
    }
    self->fd = -1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_close_obj, mp_socket_close);

static int mp_socket_connect_internal(mp_obj_t self_in, mp_obj_t address_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    struct sockaddr_storage address;
    socklen_t address_len = mp_socket_sockaddr_parse(address_in, &address);

    int ret;
    MP_OS_CALL(ret, connect, self->fd, (struct sockaddr *)&address, address_len);
    return ret;
}

static mp_obj_t mp_socket_connect(mp_obj_t self_in, mp_obj_t address_in) {
    int ret = mp_socket_connect_internal(self_in, address_in);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_connect_obj, mp_socket_connect);

static mp_obj_t mp_socket_connect_ex(mp_obj_t self_in, mp_obj_t address_in) {
    int ret = mp_socket_connect_internal(self_in, address_in);
    return MP_OBJ_NEW_SMALL_INT((ret < 0) ? errno : 0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_connect_ex_obj, mp_socket_connect_ex);

static mp_obj_t mp_socket_detach(mp_obj_t self_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    int fd = self->fd;
    self->fd = -1;
    return MP_OBJ_NEW_SMALL_INT(fd);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_detach_obj, mp_socket_detach);

static mp_obj_t mp_socket_dup(mp_obj_t self_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    int fd = dup(self->fd);
    mp_os_check_ret(fd);
    mp_obj_t args[] = { MP_OBJ_NEW_QSTR(MP_QSTR_fileno), MP_OBJ_NEW_SMALL_INT(fd) };
    return mp_socket_make_new(self->base.type, 0, 1, args);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_dup_obj, mp_socket_dup);

static mp_obj_t mp_socket_fileno(mp_obj_t self_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    return MP_OBJ_NEW_SMALL_INT(self->fd);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_fileno_obj, mp_socket_fileno);

static mp_obj_t mp_socket_getpeername(mp_obj_t self_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);

    struct sockaddr_storage address;
    socklen_t address_len = sizeof(address);
    int ret = getpeername(self->fd, (struct sockaddr *)&address, &address_len);
    mp_os_check_ret(ret);

    return mp_socket_sockaddr_format((struct sockaddr *)&address, address_len);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_getpeername_obj, mp_socket_getpeername);

static mp_obj_t mp_socket_getsockname(mp_obj_t self_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);

    struct sockaddr_storage address;
    socklen_t address_len = sizeof(address);
    int ret = getsockname(self->fd, (struct sockaddr *)&address, &address_len);
    mp_os_check_ret(ret);
    return mp_socket_sockaddr_format((struct sockaddr *)&address, address_len);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_getsockname_obj, mp_socket_getsockname);

static mp_obj_t mp_socket_getsockopt(size_t n_args, const mp_obj_t *args) {
    mp_obj_socket_t *self = mp_socket_get(args[0]);
    int level = mp_obj_get_int(args[1]);
    int name = mp_obj_get_int(args[2]);
    
    if (n_args > 3) {
        vstr_t buf;
        socklen_t buflen = mp_obj_get_int(args[3]);
        vstr_init(&buf, buflen);
        int ret = getsockopt(self->fd, level, name, buf.buf, &buflen);
        mp_os_check_ret(ret);
        buf.len = buflen;
        return mp_obj_new_bytes_from_vstr(&buf);
    }
    else {
        int value = 0;
        socklen_t len = sizeof(value);        
        int ret = getsockopt(self->fd, level, name, &value, &len);
        mp_os_check_ret(ret);
        return mp_obj_new_int(value);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_getsockopt_obj, 3, 4, mp_socket_getsockopt);

static mp_obj_t mp_socket_gettimeout(mp_obj_t self_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);

    int ret = fcntl(self->fd, F_GETFL);
    mp_os_check_ret(ret);
    if (ret & O_NONBLOCK) {
        return mp_obj_new_float(0.0);
    }

    struct timeval tv;
    size_t len = sizeof(tv);
    ret = getsockopt(self->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, &len);
    if ((ret < 0) || (!tv.tv_sec && !tv.tv_usec)) {
        return mp_const_none;
    }
    return mp_obj_new_float(tv.tv_sec + tv.tv_usec * 1e-6);
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_socket_gettimeout_obj, mp_socket_gettimeout);

static mp_obj_t mp_socket_listen(size_t n_args, const mp_obj_t *args) {
    mp_obj_socket_t *self = mp_socket_get(args[0]);
    mp_int_t backlog = TCP_DEFAULT_LISTEN_BACKLOG;
    if (n_args > 1) {
        backlog = mp_obj_get_int(args[1]);
    }
    backlog = MAX(0, backlog);

    int ret = listen(self->fd, backlog);
    mp_os_check_ret(ret);

    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_listen_obj, 1, 2, mp_socket_listen);

static mp_obj_t mp_socket_makefile(size_t n_args, const mp_obj_t *args, mp_map_t *kw_args) {
    mp_obj_socket_t *self = mp_socket_get(args[0]);
    mp_obj_t new_args[7] = {
        MP_OBJ_NEW_SMALL_INT(self->fd),  // file
        MP_OBJ_NEW_QSTR(MP_QSTR_r),  // mode
        MP_OBJ_NEW_SMALL_INT(-1),  // buffering
        mp_const_none,  // encoding
        mp_const_none,  // errors
        mp_const_none,  // newline
        mp_const_false,  // closefd
    };
    const qstr kws[] = { MP_QSTR_mode, MP_QSTR_buffering, MP_QSTR_encoding, MP_QSTR_errors, MP_QSTR_newline, 0 };
    parse_args_and_kw_map(n_args - 1, args + 1, kw_args, "|OO$OOOOO", kws, &new_args[1], &new_args[2], &new_args[3], &new_args[4], &new_args[5]);
    return mp_builtin_open(7, new_args, (mp_map_t *)&mp_const_empty_map);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(mp_socket_makefile_obj, 1, mp_socket_makefile);

static int mp_socket_read_vstr(int fd, vstr_t *vstr, size_t size, int flags, struct sockaddr *address, socklen_t *address_len) {
    vstr_hint_size(vstr, size);
    int ret;
    MP_OS_CALL(ret, recvfrom, fd, vstr_str(vstr) + vstr_len(vstr), size, flags, address, address_len);
    if (ret > 0) {
        vstr_add_len(vstr, ret);
    }
    return ret;
}

static mp_obj_t mp_socket_recvfrom_internal(size_t n_args, const mp_obj_t *args, struct sockaddr_storage *address, socklen_t *address_len) {
    mp_obj_socket_t *self = mp_socket_get(args[0]);
    size_t bufsize = mp_obj_get_int(args[1]);
    if (bufsize < 0) {
        mp_raise_ValueError(NULL);
    }
    int flags = (n_args > 2) ? mp_obj_get_int(args[2]) : 0;

    vstr_t buf;
    vstr_init(&buf, bufsize);
    int ret = mp_socket_read_vstr(self->fd, &buf, bufsize, flags, (struct sockaddr *)address, address_len);
    mp_os_check_ret(ret);

    return mp_obj_new_bytes_from_vstr(&buf);
}

static mp_obj_t mp_socket_recv(size_t n_args, const mp_obj_t *args) {
    return mp_socket_recvfrom_internal(n_args, args, NULL, NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_recv_obj, 2, 3, mp_socket_recv);

static mp_obj_t mp_socket_recvfrom(size_t n_args, const mp_obj_t *args) {
    struct sockaddr_storage address;
    socklen_t address_len;
    mp_obj_t result = mp_socket_recvfrom_internal(n_args, args, &address, &address_len);

    if (result != mp_const_none) {
        mp_obj_t items[] = {
            result,
            mp_socket_sockaddr_format((struct sockaddr *)&address, address_len),
        };
        result = mp_obj_new_tuple(2, items);
    }
    return result;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_recvfrom_obj, 2, 3, mp_socket_recvfrom);

static mp_obj_t mp_socket_recvfrom_into_internal(size_t n_args, const mp_obj_t *args, struct sockaddr_storage *address, socklen_t *address_len) {
    mp_obj_socket_t *self = mp_socket_get(args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_WRITE);
    size_t nbytes = (n_args > 2) ? mp_obj_get_int(args[2]) : bufinfo.len;
    if (nbytes < 0) {
        mp_raise_ValueError(NULL);
    }
    int flags = (n_args > 3) ? mp_obj_get_int(args[3]) : 0;

    vstr_t vstr;
    vstr_init_fixed_buf(&vstr, bufinfo.len, bufinfo.buf);
    *address_len = sizeof(*address);
    int ret = mp_socket_read_vstr(self->fd, &vstr, nbytes, flags, (struct sockaddr *)address, address_len);
    return mp_os_check_ret(ret);
}

static mp_obj_t mp_socket_recv_into(size_t n_args, const mp_obj_t *args) {
    return mp_socket_recvfrom_into_internal(n_args, args, NULL, NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_recv_into_obj, 2, 4, mp_socket_recv_into);

static mp_obj_t mp_socket_recvfrom_into(size_t n_args, const mp_obj_t *args) {
    struct sockaddr_storage address;
    socklen_t address_len;
    mp_obj_t result = mp_socket_recvfrom_into_internal(n_args, args, &address, &address_len);

    if (result != mp_const_none) {
        mp_obj_t items[] = {
            result,
            mp_socket_sockaddr_format((struct sockaddr *)&address, address_len),
        };
        result = mp_obj_new_tuple(2, items);
    }
    return result;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_recvfrom_into_obj, 2, 4, mp_socket_recvfrom_into);

static int mp_socket_write_str(int fd, const char *str, size_t len, int flags, const struct sockaddr *address, socklen_t address_len) {
    int ret;
    MP_OS_CALL(ret, sendto, fd, str, len, flags, address, address_len);
    return ret;
}

static mp_obj_t mp_socket_sendto_internal(size_t n_args, const mp_obj_t *args, mp_obj_t address_in) {
    mp_obj_socket_t *self = mp_socket_get(args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
    int flags = (n_args > 2) ? mp_obj_get_int(args[2]) : 0;

    struct sockaddr_storage address_storage;
    struct sockaddr *address = NULL;
    socklen_t address_len = 0;
    if (address_in != MP_OBJ_NULL) {    
        address = (struct sockaddr *)&address_storage; 
        address_len = mp_socket_sockaddr_parse(address_in, &address_storage);
    }
    int ret = mp_socket_write_str(self->fd, bufinfo.buf, bufinfo.len, flags, address, address_len);
    return mp_os_check_ret(ret);
}

static mp_obj_t mp_socket_send(size_t n_args, const mp_obj_t *args) {
    return mp_socket_sendto_internal(n_args, args, MP_OBJ_NULL);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_send_obj, 2, 3, mp_socket_send);

static mp_obj_t mp_socket_sendall(size_t n_args, const mp_obj_t *args) {
    mp_obj_socket_t *self = mp_socket_get(args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
    int flags = (n_args > 2) ? mp_obj_get_int(args[2]) : 0;

    while (bufinfo.len > 0) {
        int ret = mp_socket_write_str(self->fd, bufinfo.buf, bufinfo.len, flags, NULL, 0);
        mp_os_check_ret(ret);
        bufinfo.buf += ret;
        bufinfo.len -= ret;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_sendall_obj, 2, 3, mp_socket_sendall);

static mp_obj_t mp_socket_sendto(size_t n_args, const mp_obj_t *args) {
    return mp_socket_sendto_internal(n_args - 1, args, args[n_args - 1]);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_sendto_obj, 3, 4, mp_socket_sendto);

static mp_obj_t mp_socket_setsockopt(size_t n_args, const mp_obj_t *args) {
    mp_obj_socket_t *self = mp_socket_get(args[0]);
    int level = mp_obj_get_int(args[1]);
    int name = mp_obj_get_int(args[2]);
    int ret;
    if (mp_obj_is_int(args[3])) {
        int value = mp_obj_get_int(args[3]);
        ret = setsockopt(self->fd, level, name, &value, sizeof(value));
    }
    else {
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[3], &bufinfo, MP_BUFFER_READ);
        ret = setsockopt(self->fd, level, name, bufinfo.buf, bufinfo.len);
    }
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mp_socket_setsockopt_obj, 4, 4, mp_socket_setsockopt);

static mp_obj_t mp_socket_settimeout(mp_obj_t self_in, mp_obj_t value_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    int flags = fcntl(self->fd, F_GETFL);
    mp_os_check_ret(flags);
    flags &= ~O_NONBLOCK;

    struct timeval tv = { 0 };
    if (value_in != mp_const_none) {
        mp_float_t value = mp_obj_get_float(value_in);
        if (value > 0.0) {
            tv.tv_sec = value;
            tv.tv_usec = value * 1e6;
        }
        else if (value == 0.0) {
            flags |= O_NONBLOCK;
        }
    }

    int ret = fcntl(self->fd, F_SETFL, flags);
    mp_os_check_ret(ret);   
    if (!(flags & O_NONBLOCK)) {
        ret = setsockopt(self->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        mp_os_check_ret(ret);
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_settimeout_obj, mp_socket_settimeout);

static mp_obj_t mp_socket_setblocking(mp_obj_t self_in, mp_obj_t flag_in) {
    return mp_socket_settimeout(self_in, mp_obj_is_true(flag_in) ? mp_const_none : mp_obj_new_float(0.0));
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_setblocking_obj, mp_socket_setblocking);

static mp_obj_t mp_socket_shutdown(mp_obj_t self_in, mp_obj_t how_in) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    int how = mp_obj_get_int(how_in);

    int ret = shutdown(self->fd, how);
    mp_os_check_ret(ret);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mp_socket_shutdown_obj, mp_socket_shutdown);

static mp_uint_t mp_socket_stream_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    vstr_t vstr;
    vstr_init_fixed_buf(&vstr, size, buf);
    int ret = mp_socket_read_vstr(self->fd, &vstr, size, 0, NULL, NULL);
    if (ret < 0) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }
    return ret;
}

static mp_uint_t mp_socket_stream_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    mp_obj_socket_t *self = mp_socket_get(self_in);
    int ret = mp_socket_write_str(self->fd, buf, size, 0, NULL, 0);
    if (ret < 0) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }
    return ret;
}

static const mp_rom_map_elem_t mp_socket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__),         MP_ROM_PTR(&mp_socket_close_obj) },

    { MP_ROM_QSTR(MP_QSTR_accept),          MP_ROM_PTR(&mp_socket_accept_obj) },
    { MP_ROM_QSTR(MP_QSTR_bind),            MP_ROM_PTR(&mp_socket_bind_obj) },
    { MP_ROM_QSTR(MP_QSTR_close),           MP_ROM_PTR(&mp_socket_close_obj) },    
    { MP_ROM_QSTR(MP_QSTR_connect),         MP_ROM_PTR(&mp_socket_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect_ex),      MP_ROM_PTR(&mp_socket_connect_ex_obj) },
    { MP_ROM_QSTR(MP_QSTR_detach),          MP_ROM_PTR(&mp_socket_detach_obj) },
    { MP_ROM_QSTR(MP_QSTR_dup),             MP_ROM_PTR(&mp_socket_dup_obj) },
    { MP_ROM_QSTR(MP_QSTR_fileno),          MP_ROM_PTR(&mp_socket_fileno_obj) },
    { MP_ROM_QSTR(MP_QSTR_getpeername),     MP_ROM_PTR(&mp_socket_getpeername_obj) },
    { MP_ROM_QSTR(MP_QSTR_getsockname),     MP_ROM_PTR(&mp_socket_getsockname_obj) },
    { MP_ROM_QSTR(MP_QSTR_getsockopt),      MP_ROM_PTR(&mp_socket_getsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_gettimeout),      MP_ROM_PTR(&mp_socket_gettimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_listen),          MP_ROM_PTR(&mp_socket_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_makefile),        MP_ROM_PTR(&mp_socket_makefile_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv),            MP_ROM_PTR(&mp_socket_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom),        MP_ROM_PTR(&mp_socket_recvfrom_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv_into),       MP_ROM_PTR(&mp_socket_recv_into_obj) },
    { MP_ROM_QSTR(MP_QSTR_recvfrom_into),   MP_ROM_PTR(&mp_socket_recvfrom_into_obj) },
    { MP_ROM_QSTR(MP_QSTR_send),            MP_ROM_PTR(&mp_socket_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendall),         MP_ROM_PTR(&mp_socket_sendall_obj) },
    { MP_ROM_QSTR(MP_QSTR_sendto),          MP_ROM_PTR(&mp_socket_sendto_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking),     MP_ROM_PTR(&mp_socket_setblocking_obj) },
    { MP_ROM_QSTR(MP_QSTR_settimeout),      MP_ROM_PTR(&mp_socket_settimeout_obj) },
    { MP_ROM_QSTR(MP_QSTR_setsockopt),      MP_ROM_PTR(&mp_socket_setsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_shutdown),        MP_ROM_PTR(&mp_socket_shutdown_obj) },
};
static MP_DEFINE_CONST_DICT(mp_socket_locals_dict, mp_socket_locals_dict_table);

static const mp_stream_p_t mp_socket_stream_p = {
    .read = mp_socket_stream_read,
    .write = mp_socket_stream_write,
    .ioctl = mp_io_stream_ioctl,
};

MP_DEFINE_CONST_OBJ_TYPE(
    mp_type_socket,
    MP_QSTR_Socket,
    MP_TYPE_FLAG_NONE,
    make_new, mp_socket_make_new,
    attr, mp_socket_attr,
    // print, mp_socket_print,
    protocol, &mp_socket_stream_p,
    locals_dict, &mp_socket_locals_dict
    );
