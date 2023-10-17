// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "lwip/tcp.h"

#include "./tcp.h"
#include "./error.h"


STATIC struct tcp_pcb *socket_tcp_lwip_free(socket_obj_t *socket) {
    struct tcp_pcb *pcb = socket->pcb.tcp;
    socket->pcb.tcp = NULL;
    if (pcb != NULL && (pcb->state != LISTEN)) {
        tcp_arg(pcb, NULL);
        tcp_err(pcb, NULL);
        tcp_accept(pcb, NULL);
        tcp_recv(pcb, NULL);
        tcp_sent(pcb, NULL);
    }
    return pcb;
}

STATIC void socket_tcp_lwip_err(void *arg, err_t err) {
    // printf("tcp_err: err=%i\n", (int)err);
    socket_obj_t *socket = arg;
    socket->pcb.tcp = NULL;
    socket_acquire(socket);
    socket->errcode = error_lookup_table[-err];
    mp_stream_poll_signal(&socket->poll, MP_STREAM_POLL_ERR, NULL);
    socket_release(socket);
}

STATIC err_t socket_tcp_lwip_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err);

STATIC err_t socket_tcp_lwip_sent(void *arg, struct tcp_pcb *pcb, u16_t len);

STATIC err_t socket_tcp_lwip_new(socket_obj_t *socket) {
    if (socket->pcb.tcp) {
        return ERR_VAL;
    }
    
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        return ERR_MEM;
    }

    socket->pcb.tcp = pcb;
    tcp_arg(pcb, socket);
    tcp_err(pcb, socket_tcp_lwip_err);
    tcp_recv(pcb, socket_tcp_lwip_recv);
    tcp_sent(pcb, socket_tcp_lwip_sent);
    return ERR_OK;
}

STATIC err_t socket_tcp_lwip_close(socket_obj_t *socket) {
    err_t err = ERR_OK;
    if (socket->pcb.tcp) {
        struct tcp_pcb *pcb = socket_tcp_lwip_free(socket);
        err = tcp_close(pcb);
    }
    return err;
}

STATIC err_t socket_tcp_lwip_abort(socket_obj_t *socket) {
    if (socket->pcb.tcp) {
        struct tcp_pcb *pcb = socket_tcp_lwip_free(socket);
        if (socket->listening) {
            tcp_close(pcb);
        }
        else {
            tcp_abort(pcb);
        }
    }
    return ERR_OK;
}

STATIC err_t socket_tcp_lwip_bind(socket_obj_t *socket, const struct sockaddr *address) {
    err_t err = tcp_bind(socket->pcb.tcp, &address->addr, address->port);
    if (err == ERR_OK) {
        socket_acquire(socket);
        socket->local.addr = socket->pcb.tcp->local_ip;
        socket->local.port = socket->pcb.tcp->local_port;
        socket_release(socket);
    }
    return err;
}

struct socket_tcp_accept_result {
    err_t err;
    struct tcp_pcb *new_pcb;
    struct sockaddr local;
    struct sockaddr remote;
};

STATIC void socket_tcp_lwip_err_unaccepted(void *arg, err_t err) {
    // printf("tcp_err_unaccepted: err=%i\n", (int)err);
    struct pbuf *accept_arg = arg;
    struct socket_tcp_accept_result *accept_result = accept_arg->payload;
    accept_result->err = err;
    accept_result->new_pcb = NULL;
}

STATIC err_t socket_tcp_lwip_accept(void *arg, struct tcp_pcb *new_pcb, err_t err) {
    // printf("tcp_accept: local=%s:%hu", ipaddr_ntoa(&new_pcb->local_ip), new_pcb->local_port);
    // printf(", remote=%s:%hu, err=%i\n", ipaddr_ntoa(&new_pcb->remote_ip), new_pcb->remote_port, (int)err);
    socket_obj_t *socket = arg;

    struct pbuf *accept_arg = pbuf_alloc(PBUF_RAW, sizeof(struct socket_tcp_accept_result), PBUF_POOL);
    if (!accept_arg) {
        tcp_abort(new_pcb);
        return ERR_ABRT;
    }

    struct socket_tcp_accept_result *accept_result = accept_arg->payload;
    accept_result->err = ERR_OK;
    accept_result->new_pcb = new_pcb;
    accept_result->local.addr = new_pcb->local_ip;
    accept_result->local.port = new_pcb->local_port;
    accept_result->remote.addr = new_pcb->remote_ip;
    accept_result->remote.port = new_pcb->remote_port;

    tcp_arg(new_pcb, accept_arg);
    tcp_err(new_pcb, socket_tcp_lwip_err_unaccepted);
    tcp_backlog_delayed(new_pcb);

    mp_uint_t events = MP_STREAM_POLL_RD;
    int errcode;
    socket_acquire(socket);
    mp_uint_t ret = socket_push(socket, &accept_arg, sizeof(accept_arg), &errcode);    
    if (ret == MP_STREAM_ERROR) {
        socket->errcode = errcode;
        events = MP_STREAM_POLL_ERR;
    }
    mp_stream_poll_signal(&socket->poll, events, NULL);
    socket_release(socket);

    if (ret == MP_STREAM_ERROR) {
        tcp_abort(new_pcb);
        pbuf_free(accept_arg);
        return ERR_ABRT;
    }
    return ERR_OK;
}

STATIC err_t socket_tcp_lwip_listen(socket_obj_t *socket, u8_t backlog) {
    err_t err = ERR_OK;
    struct tcp_pcb *new_pcb = tcp_listen_with_backlog_and_err(socket->pcb.tcp, backlog, &err);
    if (new_pcb) {
        tcp_accept(new_pcb, socket_tcp_lwip_accept);       
        socket->pcb.tcp = new_pcb;
    }
    return err;
}

STATIC err_t socket_tcp_lwip_connected(void *arg, struct tcp_pcb *pcb, err_t err) {
    // err is always ERR_OK, failure returned through tcp_err
    // printf("tcp_connected: local=%s:%hu", ipaddr_ntoa(&pcb->local_ip), pcb->local_port);
    // printf(", remote=%s:%hu, err=%i\n", ipaddr_ntoa(&pcb->remote_ip), pcb->remote_port, (int)err);
    socket_obj_t *socket = arg;
    socket_acquire(socket);
    socket->connected = 1;
    socket->local.addr = pcb->local_ip;
    socket->local.port = pcb->local_port;
    socket->remote.addr = pcb->remote_ip;
    socket->remote.port = pcb->remote_port;
    mp_stream_poll_signal(&socket->poll, MP_STREAM_POLL_RD | MP_STREAM_POLL_WR, NULL);
    socket_release(socket);
    return ERR_OK;
}

STATIC err_t socket_tcp_lwip_connect(socket_obj_t *socket, const struct sockaddr *address) {
    return tcp_connect(socket->pcb.tcp, &address->addr, address->port, socket_tcp_lwip_connected);
}

STATIC err_t socket_tcp_lwip_recved(socket_obj_t *socket, u16_t len) {
    if (socket->pcb.tcp) {
        tcp_recved(socket->pcb.tcp, len);
    }
    return ERR_OK;
}

STATIC err_t socket_tcp_lwip_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    // printf("tcp_recv: local=%s:%hu", ipaddr_ntoa(&pcb->local_ip), pcb->local_port);
    // printf(", remote=%s:%hu, len=%i, err=%i\n", ipaddr_ntoa(&pcb->remote_ip), pcb->remote_port, p ? (int)p->tot_len : -1, (int)err);
    socket_obj_t *socket = arg;
    socket_acquire(socket);
    mp_uint_t events = socket_empty(socket) ? MP_STREAM_POLL_RD : 0;
    if (p) {
        socket_push_pbuf(socket, p);
    }
    else {
        events |= MP_STREAM_POLL_HUP;
        socket->peer_closed = 1;
    }
    mp_stream_poll_signal(&socket->poll, events, NULL);
    socket_release(socket);    
    return ERR_OK;
}

STATIC err_t socket_tcp_lwip_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    // printf("tcp_sent: local=%s:%hu", ipaddr_ntoa(&pcb->local_ip), pcb->local_port);
    // printf(", remote=%s:%hu, len=%hu\n", ipaddr_ntoa(&pcb->remote_ip), pcb->remote_port, len);
    socket_obj_t *socket = arg;
    if (tcp_sndbuf(socket->pcb.tcp) <= len) {
        socket_acquire(socket);
        mp_stream_poll_signal(&socket->poll, MP_STREAM_POLL_WR, NULL);
        socket_release(socket);
    }
    return ERR_OK;
}

STATIC err_t socket_tcp_lwip_sendto(socket_obj_t *socket, socket_sendto_args_t *args) {
    u16_t len = LWIP_MIN(args->len, tcp_sndbuf(socket->pcb.tcp));
    if (args->address != NULL) {
        return ERR_VAL;
    }
    u8_t apiflags = TCP_WRITE_FLAG_COPY | (len < args->len ? TCP_WRITE_FLAG_MORE : 0);
    err_t err = tcp_write(socket->pcb.tcp, args->buf, len, apiflags);
    if (err == ERR_OK) {
        args->len = len;
    }
    return err;
}

STATIC err_t socket_tcp_lwip_shutdown(socket_obj_t *socket, int shut_rx, int shut_tx) {
    err_t err = tcp_shutdown(socket->pcb.tcp, shut_rx, shut_tx);
    return err;
}

STATIC err_t socket_tcp_lwip_output(socket_obj_t *socket) {
    if (!socket->pcb.tcp) {
        return ERR_ARG;
    }
    return tcp_output(socket->pcb.tcp);
}

STATIC err_t socket_tcp_lwip_new_accept(socket_obj_t *socket, struct pbuf *accept_arg, socket_obj_t *new_socket) {
    struct socket_tcp_accept_result *accept_result = accept_arg->payload;
    struct tcp_pcb *new_pcb = accept_result->new_pcb;
    if (new_socket) {
        if (new_pcb) {
            new_socket->pcb.tcp = new_pcb;
            tcp_arg(new_pcb, new_socket);
            tcp_err(new_pcb, socket_tcp_lwip_err);
            tcp_recv(new_pcb, socket_tcp_lwip_recv);
            tcp_sent(new_pcb, socket_tcp_lwip_sent);
            tcp_backlog_accepted(new_pcb);
        }
        new_socket->errcode = error_lookup_table[-accept_result->err];
        new_socket->connected = 1;
        new_socket->local = accept_result->local;
        new_socket->remote = accept_result->remote;
    }
    else if (new_pcb) {
        tcp_abort(new_pcb);
    }
    pbuf_free(accept_arg);
    return ERR_OK;
}

mp_uint_t socket_tcp_accept(socket_obj_t *socket, socket_obj_t **new_socket, int *errcode) {
    if (!socket->listening) {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    } 

    struct pbuf *accept_arg;
    mp_uint_t ret = socket_pop_block(socket, &accept_arg, sizeof(accept_arg), errcode);
    if (ret == MP_STREAM_ERROR || ret == 0) {
        return ret;
    }

    *new_socket = socket_new(socket->base.type, socket->func);

    LOCK_TCPIP_CORE();
    err_t err = socket_tcp_lwip_new_accept(socket, accept_arg, *new_socket);
    UNLOCK_TCPIP_CORE();
    if (socket_lwip_err(err, errcode)) {
        return MP_STREAM_ERROR;
    }
    return 0;
}

mp_uint_t socket_tcp_recvfrom(socket_obj_t *socket, void *buf, size_t len, struct sockaddr *address, int *errcode) {
    if (address != NULL) {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }
    
    socket_acquire(socket);
    int connected = socket->connected && !socket->listening;
    socket_release(socket);
    if (!connected) {
        *errcode = MP_ENOTCONN;
        return MP_STREAM_ERROR;
    }        

    mp_uint_t ret = socket_pop_block(socket, buf, len, errcode);
    if (ret != MP_STREAM_ERROR && ret != 0) {
        LOCK_TCPIP_CORE();
        socket_tcp_lwip_recved(socket, ret);
        UNLOCK_TCPIP_CORE();
    }
    return ret;
}

void socket_tcp_cleanup(socket_obj_t *socket, struct pbuf *p, u16_t offset, u16_t len) {
    if (!socket->listening) {
        return;
    }

    struct pbuf *accept_arg;
    while (p != NULL && len >= sizeof(accept_arg)) {
        u16_t br = pbuf_copy_partial(p, &accept_arg, sizeof(accept_arg), offset);
        assert(br == sizeof(accept_arg));
        p = pbuf_skip(p, offset + br, &offset);
        len -= br;

        LOCK_TCPIP_CORE();
        socket_tcp_lwip_new_accept(socket, accept_arg, NULL);
        UNLOCK_TCPIP_CORE();
    }
}

const struct socket_vtable socket_tcp_vtable = {
    .pcb_type = LWIP_PCB_TCP,
    .lwip_new = socket_tcp_lwip_new,
    .lwip_close = socket_tcp_lwip_close,
    .lwip_abort = socket_tcp_lwip_abort,
    .lwip_bind = socket_tcp_lwip_bind,
    .lwip_listen = socket_tcp_lwip_listen,
    .lwip_connect = socket_tcp_lwip_connect,
    .lwip_sendto = socket_tcp_lwip_sendto,
    .lwip_shutdown = socket_tcp_lwip_shutdown,
    .lwip_output = socket_tcp_lwip_output,
    
    .socket_accept = socket_tcp_accept,
    .socket_recvfrom = socket_tcp_recvfrom,
    .socket_cleanup = socket_tcp_cleanup,
};