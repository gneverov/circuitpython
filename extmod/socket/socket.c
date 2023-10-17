// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <memory.h>

#include "./socket.h"
#include "./error.h"
#include "shared/netutils/netutils.h"


void sokcet_sockaddr_parse(mp_obj_t address_in, struct sockaddr *address) {
    address->port = netutils_parse_inet_addr(address_in, (uint8_t *)&address->addr, NETUTILS_BIG);
}

mp_obj_t socket_sockaddr_format(const struct sockaddr *address) {
    return netutils_format_inet_addr((uint8_t *)&address->addr, address->port, NETUTILS_BIG);
}

void socket_lwip_raise(err_t err) {
    if (err != ERR_OK) {
        mp_raise_OSError(error_lookup_table[-err]);
    }
}

bool socket_lwip_err(err_t err, int *errcode) {
    if (err != ERR_OK) {
        *errcode = error_lookup_table[-err];
        return true;
    }
    return false;
}

void socket_acquire(socket_obj_t *socket) {
    if (!xSemaphoreTake(socket->mutex, portMAX_DELAY)) {
        assert(0);
    }
}

void socket_release(socket_obj_t *socket) {
    if (!xSemaphoreGive(socket->mutex)) {
        assert(0);
    }
}

socket_obj_t *socket_new(const mp_obj_type_t *type, const struct socket_vtable *vtable) {
    socket_obj_t *self = m_new_obj_with_finaliser(socket_obj_t);
    memset(self, 0, sizeof(socket_obj_t));
    self->base.type = type;
    self->func = vtable;
    self->timeout = portMAX_DELAY;
    mp_stream_poll_init(&self->poll);
    self->mutex = xSemaphoreCreateMutexStatic(&self->mutex_buffer);
    return self;
}

void socket_deinit(socket_obj_t *socket) {
    vSemaphoreDelete(socket->mutex);
}

void socket_call_cleanup(socket_obj_t *socket) {
    if (!socket->rx_data) {
        return;
    }
    if (socket->func->socket_cleanup) {
        socket->func->socket_cleanup(socket, socket->rx_data, socket->rx_offset, socket->rx_len);
    }
    pbuf_free(socket->rx_data);
    socket->rx_data = NULL;
}

STATIC struct pbuf *pbuf_advance(struct pbuf *p, u16_t *offset, u16_t len) {
    struct pbuf *new_p = pbuf_skip(p, *offset + len, offset);
    if (new_p != p) {
        pbuf_ref(new_p);
        pbuf_free(p);
    }
    return new_p;
}

STATIC struct pbuf *pbuf_concat(struct pbuf *p, struct pbuf *new_p) {
    if (p) {
        if (new_p) {
            pbuf_cat(p, new_p);
        }
        return p;
    }
    else {
        return new_p;
    }
}

STATIC struct pbuf *pbuf_grow(struct pbuf *p, u16_t new_len) {
    ssize_t delta = new_len - (p ? p->tot_len : 0);
    if (delta > 0) {
        struct pbuf *new_p = pbuf_alloc(PBUF_RAW, delta, PBUF_RAM);
        p = pbuf_concat(p, new_p);
    }
    return p;
}

bool socket_empty(socket_obj_t *socket) {
    return socket->rx_data == NULL || socket->rx_len == 0;
}

mp_uint_t socket_pop_nonblock(mp_obj_t stream_obj, void *buf, mp_uint_t size, int *errcode) {
    socket_obj_t *socket = MP_OBJ_TO_PTR(stream_obj);
    socket_acquire(socket);
    int errcode_copy = socket->errcode;
    struct pbuf *rx_data = socket->rx_data;
    size_t rx_len = socket->rx_len;
    int peer_closed = socket->peer_closed;
    socket_release(socket);

    if (errcode_copy) {
        *errcode = errcode_copy;
        return MP_STREAM_ERROR;
    }

    if (rx_data != NULL && rx_len > 0) {
        u16_t br = pbuf_copy_partial(rx_data, buf, MIN(size, rx_len), socket->rx_offset);
        if (br == 0) {
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
        }

        socket_acquire(socket);
        socket->rx_data = pbuf_advance(socket->rx_data, &socket->rx_offset, br);
        socket->rx_len -= br;
        socket_release(socket);
        return br;
    }

    if (peer_closed) {
        return 0;
    }
    
    *errcode = MP_EAGAIN;
    return MP_STREAM_ERROR;
}

mp_uint_t socket_pop_block(socket_obj_t *socket, void *buf, mp_uint_t size, int *errcode) {
    return mp_poll_block(MP_OBJ_FROM_PTR(socket), buf, size, errcode, socket_pop_nonblock, MP_STREAM_POLL_RD, socket->timeout, false);
}

mp_uint_t socket_push(socket_obj_t *socket, const void *buf, mp_uint_t size, int *errcode) {
    if (socket->errcode) {
        *errcode = socket->errcode;
        return MP_STREAM_ERROR;
    }

    u16_t offset = socket->rx_offset + socket->rx_len;
    u16_t new_len = (offset + size + 255) & ~255;
    socket->rx_data = pbuf_grow(socket->rx_data, new_len);

    if (pbuf_take_at(socket->rx_data, buf, size, offset) != ERR_OK) {
        *errcode = MP_ENOMEM;
        return MP_STREAM_ERROR;
    }
    socket->rx_len += size;
    return 0;
}

void socket_push_pbuf(socket_obj_t *socket, struct pbuf *p) {
    assert(p);
    socket->rx_len += p->tot_len;
    socket->rx_data = pbuf_concat(socket->rx_data, p);
    assert(socket->rx_offset + socket->rx_len == socket->rx_data->tot_len);
}