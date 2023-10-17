// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"

#include "py/obj.h"
#include "py/stream_poll.h"


struct sockaddr {
    u16_t port;  
    ip_addr_t addr;
};

void sokcet_sockaddr_parse(mp_obj_t address_in, struct sockaddr *address);

mp_obj_t socket_sockaddr_format(const struct sockaddr *address);

struct socket_vtable;

typedef struct {
    mp_obj_base_t base;
    const struct socket_vtable *func;
    union {
        struct tcp_pcb *tcp;
        struct udp_pcb *udp;
        struct pbuf *dns;
    } pcb;
    int connected : 1;
    int peer_closed : 1;    

    int listening : 1;
    int connecting : 1;
    int user_closed : 1;
    TickType_t timeout;

    struct sockaddr local;
    struct sockaddr remote;
    int errcode;

    struct pbuf *rx_data;
    uint16_t rx_offset;
    uint16_t rx_len;
   
    mp_stream_poll_t poll;

    SemaphoreHandle_t mutex;
    StaticSemaphore_t mutex_buffer;
} socket_obj_t;

void socket_acquire(socket_obj_t *socket);

void socket_release(socket_obj_t *socket);

socket_obj_t *socket_new(const mp_obj_type_t *type, const struct socket_vtable *vtable);

void socket_deinit(socket_obj_t *socket);

bool socket_empty(socket_obj_t *socket);

mp_uint_t socket_pop_nonblock(mp_obj_t stream_obj, void *buf, mp_uint_t size, int *errcode);

mp_uint_t socket_pop_block(socket_obj_t *socket, void *buf, mp_uint_t size, int *errcode);

mp_uint_t socket_push(socket_obj_t *socket, const void *buf, mp_uint_t size, int *errcode);

void socket_push_pbuf(socket_obj_t *socket, struct pbuf *p);

enum lwip_pcb {
    LWIP_PCB_DNS,
    LWIP_PCB_RAW,
    LWIP_PCB_TCP,
    LWIP_PCB_UDP,
};

typedef struct {
        const void *buf;
        u16_t len;
        const struct sockaddr *address;
} socket_sendto_args_t;

struct socket_vtable {
    enum lwip_pcb pcb_type;
    err_t (*lwip_new)(socket_obj_t *socket);
    err_t (*lwip_close)(socket_obj_t *socket);
    err_t (*lwip_abort)(socket_obj_t *socket);
    err_t (*lwip_bind)(socket_obj_t *socket, const struct sockaddr *address);
    err_t (*lwip_listen)(socket_obj_t *socket, u8_t backlog);
    err_t (*lwip_connect)(socket_obj_t *socket, const struct sockaddr *address);
    err_t (*lwip_sendto)(socket_obj_t *socket, socket_sendto_args_t *args);
    err_t (*lwip_shutdown)(socket_obj_t *socket, int shut_rx, int shut_tx);
    err_t (*lwip_output)(socket_obj_t *socket);

    mp_uint_t (*socket_accept)(socket_obj_t *socket, socket_obj_t **new_socket, int *errcode);
    mp_uint_t (*socket_recvfrom)(socket_obj_t *socket, void *buf, size_t len, struct sockaddr *addr, int *errcode);
    void (*socket_cleanup)(socket_obj_t *socket, struct pbuf *p, u16_t offset, u16_t len);
};

void socket_lwip_raise(err_t err);

bool socket_lwip_err(err_t err, int *errcode);

void socket_call_cleanup(socket_obj_t *socket);