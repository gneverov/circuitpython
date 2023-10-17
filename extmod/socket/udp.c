// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "lwip/udp.h"

#include "./udp.h"
#include "./error.h"


STATIC void socket_udp_lwip_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port);

STATIC err_t socket_udp_lwip_new(socket_obj_t *socket) {
    if (socket->pcb.udp) {
        return ERR_VAL;
    }

    struct udp_pcb *pcb = udp_new();
    if (!pcb) {
        return ERR_MEM;
    }
    
    socket->pcb.udp = pcb;
    udp_recv(pcb, socket_udp_lwip_recv, socket);
    return ERR_OK;
}

STATIC err_t socket_udp_lwip_abort(socket_obj_t *socket) {
    if (socket->pcb.udp) {
        udp_recv(socket->pcb.udp, NULL, NULL);
        udp_remove(socket->pcb.udp);
        socket->pcb.udp = NULL;
    }
    return ERR_OK;
}

STATIC err_t socket_udp_lwip_bind(socket_obj_t *socket, const struct sockaddr *address) {
    err_t err = udp_bind(socket->pcb.udp, &address->addr, address->port);
    if (err == ERR_OK) {
        socket_acquire(socket);
        socket->local.addr = socket->pcb.udp->local_ip;
        socket->local.port = socket->pcb.udp->local_port;
        socket_release(socket);
    }
    return err;
}

STATIC err_t socket_udp_lwip_connect(socket_obj_t *socket, const struct sockaddr *address) {
    struct udp_pcb *pcb = socket->pcb.udp;
    err_t err = udp_connect(pcb, &address->addr, address->port);
    if (err == ERR_OK) {
        socket_acquire(socket);
        socket->connected = 1;
        socket->local.addr = pcb->local_ip;
        socket->local.port = pcb->local_port;
        socket->remote.addr = pcb->remote_ip;
        socket->remote.port = pcb->remote_port;
        socket_release(socket);
    }
    return err;
}

struct socket_udp_recv_result {
    struct pbuf *p;
    struct sockaddr remote;
};

STATIC void socket_udp_lwip_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
    printf("udp_recv: local=%s:%hu", ipaddr_ntoa(&pcb->local_ip), pcb->local_port);
    printf(", remote=%s:%hu, len=%i\n", ipaddr_ntoa(&pcb->remote_ip), pcb->remote_port, p ? (int)p->tot_len : -1);
    socket_obj_t *socket = arg;
    socket_acquire(socket);
    mp_uint_t events = socket_empty(socket) ? MP_STREAM_POLL_RD : 0;

    struct socket_udp_recv_result recv_result;
    recv_result.p = p;
    recv_result.remote.addr = *addr;
    recv_result.remote.port = port;

    int errcode;
    mp_uint_t ret = socket_push(socket, &recv_result, sizeof(recv_result), &errcode);
    if (ret == MP_STREAM_ERROR || ret == 0) {
        events |= MP_STREAM_ERROR;
        pbuf_free(p);
    }

    mp_stream_poll_signal(&socket->poll, events, NULL);
    socket_release(socket);    
}

STATIC err_t socket_udp_lwip_sendto(socket_obj_t *socket, socket_sendto_args_t *args) {
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, args->len, PBUF_RAM);
    if (!p) {
        return ERR_MEM;
    }
    err_t err = pbuf_take(p, args->buf, args->len);
    assert(err == ERR_OK);
    
    if (args->address == NULL) {
        err = udp_send(socket->pcb.udp, p);    
    }
    else {
        err = udp_sendto(socket->pcb.udp, p, &args->address->addr, args->address->port);
    }
    pbuf_free(p);
    return err;
}

mp_uint_t socket_udp_recvfrom(socket_obj_t *socket, void *buf, size_t len, struct sockaddr *address, int *errcode) {
    struct socket_udp_recv_result recv_result;
    mp_uint_t ret = socket_pop_block(socket, &recv_result, sizeof(recv_result), errcode);
    if (ret != MP_STREAM_ERROR && ret != 0) {
        ret = pbuf_copy_partial(recv_result.p, buf, len, 0);
        if (address) {
            *address = recv_result.remote;
        }
        pbuf_free(recv_result.p);
    }
    return ret;
}

void socket_udp_cleanup(socket_obj_t *socket, struct pbuf *p, u16_t offset, u16_t len) {
    struct socket_udp_recv_result recv_result;
    while (p != NULL && len >= sizeof(recv_result)) {
        u16_t br = pbuf_copy_partial(p, &recv_result, sizeof(recv_result), offset);
        assert(br == sizeof(recv_result));
        p = pbuf_skip(p, offset + br, &offset);
        len -= br;

        pbuf_free(recv_result.p);
    }
}

const struct socket_vtable socket_udp_vtable = {
    .pcb_type = LWIP_PCB_UDP,

    .lwip_new = socket_udp_lwip_new,
    .lwip_close = socket_udp_lwip_abort,
    .lwip_abort = socket_udp_lwip_abort,
    .lwip_bind = socket_udp_lwip_bind,
    .lwip_connect = socket_udp_lwip_connect,
    .lwip_sendto = socket_udp_lwip_sendto,
    
    .socket_recvfrom = socket_udp_recvfrom,
    .socket_cleanup = socket_udp_cleanup,
};