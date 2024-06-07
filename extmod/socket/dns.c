// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <string.h>

#include "lwip/dns.h"

#include "./dns.h"


struct socket_dns_result {
    ip_addr_t addr;
    u16_t hostname_len;
};

static err_t socket_dns_lwip_new(socket_obj_t *socket) {  
    if (socket->pcb.dns) {
        return ERR_VAL;
    }
    
    struct pbuf *pcb = pbuf_alloc(PBUF_RAW, sizeof(socket_obj_t*), PBUF_RAM);
    if (!pcb) {
        return ERR_MEM;
    }

    socket->pcb.dns = pcb;
    *(socket_obj_t **)pcb->payload = socket;
    return ERR_OK;
}

static err_t socket_dns_lwip_abort(socket_obj_t *socket) {
    struct pbuf *pcb = socket->pcb.dns;
    if (pcb) {
        *(socket_obj_t **)pcb->payload = NULL;
        pbuf_free(pcb);
        socket->pcb.dns = NULL;
    }
    return ERR_OK;
}

static void socket_dns_lwip_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    // printf("dns_found: name=%s, found=%s\n", name, ipaddr ? ipaddr_ntoa(ipaddr) : "");
    struct pbuf *pcb = callback_arg;
    socket_obj_t *socket = *(socket_obj_t **)pcb->payload;
    pbuf_free(pcb);

    if (socket == NULL) {
        return;
    }

    struct socket_dns_result result = { 0 };
    if (ipaddr) {
        result.addr = *ipaddr;
    }
    result.hostname_len = strlen(name);

    socket_acquire(socket);
    int errcode;
    if (socket_push(socket, &result, sizeof(struct socket_dns_result), &errcode) == MP_STREAM_ERROR) {
        goto _catch;
    }
    if (socket_push(socket, name, result.hostname_len, &errcode) == MP_STREAM_ERROR) {
        goto _catch;
    }
    mp_stream_poll_signal(&socket->poll, MP_STREAM_POLL_RD, NULL);
    goto _finally;

_catch:
    socket->errcode = errcode;
    mp_stream_poll_signal(&socket->poll, MP_STREAM_POLL_ERR, NULL);

_finally:
    socket_release(socket);
}

static err_t socket_dns_lwip_sendto(socket_obj_t *socket, socket_sendto_args_t *args) {
    const char* hostname = args->buf;

    if (args->address != NULL) {
        return ERR_ARG;
    }
    
    pbuf_ref(socket->pcb.dns);
    ip_addr_t addr;
    err_t err = dns_gethostbyname(hostname, &addr, socket_dns_lwip_found, socket->pcb.dns);

    if (err == ERR_OK ) {
        socket_dns_lwip_found(hostname, &addr, socket->pcb.dns);
    } 
    else if (err == ERR_INPROGRESS) {
        err = ERR_OK;
    }
    return err;
}

mp_uint_t socket_dns_recvfrom(socket_obj_t *socket, void *buf, size_t len, struct sockaddr *address, int *errcode) {
    if (address == NULL) {
        *errcode = MP_EINVAL;
        return MP_STREAM_ERROR;
    }
    struct socket_dns_result dns_result;
    mp_uint_t ret = socket_pop_block(socket, &dns_result, sizeof(dns_result), errcode);
    if (ret != MP_STREAM_ERROR) {
        address->addr = dns_result.addr;
        address->port = 0;
        ret = socket_pop_nonblock(socket, buf, MIN(dns_result.hostname_len, len), errcode);
        assert(ret == dns_result.hostname_len);
    }
    return ret;
}

const struct socket_vtable socket_dns_vtable = {
    .pcb_type = LWIP_PCB_DNS,

    .lwip_new = socket_dns_lwip_new,
    .lwip_close = socket_dns_lwip_abort,
    .lwip_abort = socket_dns_lwip_abort,
    .lwip_sendto = socket_dns_lwip_sendto,
    
    .socket_recvfrom = socket_dns_recvfrom,
};