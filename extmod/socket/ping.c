// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <time.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/icmp.h"
#include "lwip/inet_chksum.h"
#include "lwip/prot/ip4.h"
#include "lwip/raw.h"

#include "extmod/socket/socket.h"
#include "extmod/socket/ping.h"
#include "py/poll.h"
#include "py/runtime.h"
#include "shared/netutils/netutils.h"

// TTL for ping requests (0 means default)
#define PING_TTL 0

// Value of ICMP ID field for ping (arbitrary)
#define PING_ID 0x1

// Length of payload in ping request
#define PING_PAYLOAD_LEN 32

// Timeout waiting for ping response in milliseconds
#define PING_RX_TIMEOUT 3000

// Time between sending ping requests in milliseconds
#define PING_INTERVAL 1000

// The number of ping requests to send
#define PING_COUNT 4

// Global ping sequence number counter protected by lwip lock
static u16_t ping_seqno;

typedef struct ping_socket {
    TaskHandle_t task;
    struct raw_pcb *pcb;
    clock_t begin;
    ip_addr_t addr;
    u16_t seqno;
    u8_t ttl;
    clock_t end;
} ping_socket_t;

static u8_t ping_socket_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr);

static err_t ping_socket_new(ping_socket_t *socket, u8_t ttl) {
    socket->task = xTaskGetCurrentTaskHandle();
    LOCK_TCPIP_CORE();
    struct raw_pcb *pcb = raw_new(IP_PROTO_ICMP);
    if (pcb) {
        if (ttl) {
            pcb->ttl = ttl;
        }
        raw_recv(pcb, ping_socket_recv, socket);
        raw_bind(pcb, IP_ADDR_ANY);
    }
    UNLOCK_TCPIP_CORE();
    if (!pcb) {
        return ERR_MEM;
    }
    socket->pcb = pcb;

    return ERR_OK;
}

static err_t ping_socket_abort(ping_socket_t *socket) {
    if (socket->pcb) {
        raw_remove(socket->pcb);
        socket->pcb = NULL;
    }
    return ERR_OK;
}

static err_t ping_socket_sendto(ping_socket_t *socket, const void *buf, u16_t len, ip_addr_t addr) {
    struct pbuf *p = pbuf_alloc(PBUF_IP, sizeof(struct icmp_echo_hdr) + len, PBUF_RAM);
    if (!p) {
        return ERR_MEM;
    }

    u16_t seqno = ++ping_seqno;
    struct icmp_echo_hdr *hdr = p->payload;
    ICMPH_TYPE_SET(hdr, ICMP_ECHO);
    ICMPH_CODE_SET(hdr, 0);
    hdr->chksum = 0;
    hdr->id = PING_ID;
    hdr->seqno = lwip_htons(seqno);

    err_t err = pbuf_take_at(p, buf, len, sizeof(struct icmp_echo_hdr));
    assert(err == ERR_OK);

    hdr->chksum = inet_chksum_pbuf(p);

    err = raw_sendto(socket->pcb, p, &addr);
    socket->seqno = seqno;
    socket->ttl = 0;
    socket->begin = clock();
    socket->end = 0;    
    pbuf_free(p);
    return err;
}

static u8_t ping_socket_recv(void *arg, struct raw_pcb *pcb, struct pbuf *p, const ip_addr_t *addr) {
    // printf("ping_recv: local=%s", ipaddr_ntoa(&pcb->local_ip));
    // printf(", remote=%s, len=%i\n", ipaddr_ntoa(addr), p ? (int)p->tot_len : -1);
    ping_socket_t *socket = arg;
    
    struct ip_hdr *ip_hdr = p->payload;
    struct icmp_echo_hdr hdr;
    u16_t hdr_len = pbuf_copy_partial(p, &hdr, sizeof(hdr), IPH_HL_BYTES(ip_hdr));
    if ((hdr_len < sizeof(hdr)) || (hdr.id != PING_ID) || (lwip_ntohs(IPH_OFFSET(ip_hdr)) & IP_MF)) {
        return 0;
    }
    if (socket->seqno == lwip_ntohs(hdr.seqno)) {
        socket->addr = *addr;
        socket->ttl = IPH_TTL(ip_hdr);
        socket->end = clock();
        xTaskNotifyGive(socket->task);
    }
    pbuf_free(p);
    return 1;
}

typedef struct ping_context {
    nlr_jump_callback_node_t nlr_callback;
    ping_socket_t socket;
    u8_t payload[];
} ping_context_t;

static void ping_nlr_callback(void *ctx) {
    ping_context_t *ping = ctx - offsetof(ping_context_t, nlr_callback);
    LOCK_TCPIP_CORE();
    ping_socket_abort(&ping->socket);
    UNLOCK_TCPIP_CORE();
    mem_free(ping);
}

static bool ping_send(ping_socket_t *socket, const void *buf, size_t len, ip_addr_t addr, uint32_t timeout_ms) {
    ulTaskNotifyValueClear(NULL, -1);
    LOCK_TCPIP_CORE();
    err_t err = ping_socket_sendto(socket, buf, len, addr);
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);

    TickType_t timeout = pdMS_TO_TICKS(timeout_ms);
    while (mp_ulTaskNotifyTake(pdTRUE, &timeout)) {
        LOCK_TCPIP_CORE();
        bool done = socket->end;
        UNLOCK_TCPIP_CORE();
        if (done) {
            return true;
        }
    }
    return false;
}

extern const mp_obj_module_t socket_module;

mp_obj_t ping_ping(mp_obj_t dest_in) {
    mp_obj_t dest[] = { NULL, NULL, dest_in };
    mp_load_method(MP_OBJ_FROM_PTR(&socket_module), MP_QSTR_gethostbyname, dest);
    mp_obj_t addr_in = mp_call_method_n_kw(1, 0, dest);
    ip_addr_t addr;
    netutils_parse_ipv4_addr(addr_in, (uint8_t *)&addr, NETUTILS_BIG);
    size_t remaining = PING_COUNT;

    ping_context_t *ping = mem_calloc(1, sizeof(ping_context_t) + PING_PAYLOAD_LEN);
    if (!ping) {
        mp_raise_msg(&mp_type_MemoryError, NULL);
    }
    nlr_push_jump_callback(&ping->nlr_callback, ping_nlr_callback);

    ping_socket_t *socket = &ping->socket;
    LOCK_TCPIP_CORE();
    err_t err = ping_socket_new(socket, PING_TTL);
    UNLOCK_TCPIP_CORE();
    socket_lwip_raise(err);

    for (size_t i = 0; i < PING_PAYLOAD_LEN; i++) {
        ping->payload[i] = 'a' + (i & 0x1f);
    }
    
    while (remaining > 0) {
        if (ping_send(socket, ping->payload, PING_PAYLOAD_LEN, addr, PING_RX_TIMEOUT)) {
            char addr_str[IP4ADDR_STRLEN_MAX];
            ip4addr_ntoa_r(&socket->addr, addr_str, IP4ADDR_STRLEN_MAX);
            int time = (socket->end - socket->begin) * 1000 / CLOCKS_PER_SEC;
            printf("Reply from %s: bytes=%u time=%dms TTL=%hhu\n", addr_str, PING_PAYLOAD_LEN, time, socket->ttl);
            mp_vTaskDelay(pdMS_TO_TICKS(PING_INTERVAL));
        }
        else {
            puts("Request timed out.");
        }
        remaining--;        
    }
    nlr_pop_jump_callback(true);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(ping_ping_obj, ping_ping);
