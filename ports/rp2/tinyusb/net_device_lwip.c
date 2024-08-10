// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "tinyusb/net_device_lwip.h"

#if CFG_TUD_ECM_RNDIS || CFG_TUD_NCM
#include <errno.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"

#include "tinyusb/tusb_lock.h"


/* lwip context */
static struct netif netif_data;

/* this is used by this code, ./class/net/net_driver.c, and usb_descriptors.c */
/* ideally speaking, this should be generated from the hardware's unique ID (if available) */
/* it is suggested that the first byte is 0x02 to indicate a link-local address */
uint8_t tud_network_mac_address[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x00};

static struct pbuf *tx_queue_head = NULL;
static struct pbuf **tx_queue_tail = &tx_queue_head;
static SemaphoreHandle_t tx_queue_mutex;

static struct pbuf *pbuf_dequeue(struct pbuf *p) {
    struct pbuf *next = pbuf_skip(p, p->tot_len, NULL);
    pbuf_ref(next);
    pbuf_free(p);
    return next;
}

static void tud_network_output(void *param) {
    if (!xSemaphoreTake(tx_queue_mutex, portMAX_DELAY)) {
        return;
    }

    bool tx_queue_empty = true;
    if (!tx_queue_head) {
        goto _finally;
    }

    /* if TinyUSB isn't ready, we must signal back to lwip that there is nothing we can do */
    if (!tud_ready()) {
        pbuf_free(tx_queue_head);
        tx_queue_head = NULL;
        tx_queue_tail = &tx_queue_head;
        goto _finally;
    }

    /* if the network driver can accept another packet */
    if (tud_network_can_xmit(tx_queue_head->tot_len)) {
        /* we make it happen */
        tud_network_xmit(tx_queue_head, 0);

        tx_queue_head = pbuf_dequeue(tx_queue_head);
        if (!tx_queue_head) {
            tx_queue_tail = &tx_queue_head;
            goto _finally;
        }
    }

    tx_queue_empty = false;

_finally:
    xSemaphoreGive(tx_queue_mutex);
    if (!tx_queue_empty) {
        tud_callback(tud_network_output, NULL);
    }
}

static err_t tud_network_lwip_output(struct netif *netif, struct pbuf *p) {
    if (!xSemaphoreTake(tx_queue_mutex, portMAX_DELAY)) {
        return ERR_IF;
    }

    bool tx_queue_empty = !tx_queue_head;
    *tx_queue_tail = p;
    tx_queue_tail = &p->next;
    pbuf_ref(p);

    xSemaphoreGive(tx_queue_mutex);

    if (tx_queue_empty) {
        tud_callback(tud_network_output, NULL);
    }
    return ERR_OK;
}

static err_t tud_network_lwip_netif_init(struct netif *netif) {
    LWIP_ASSERT("netif != NULL", (netif != NULL));
    netif->mtu = CFG_TUD_NET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_UP;
    netif->state = NULL;
    netif->name[0] = 's';
    netif->name[1] = 'l';
    netif->linkoutput = tud_network_lwip_output;
    netif->output = etharp_output;
    #if LWIP_IPV6
    netif->output_ip6 = ethip6_output;
    #endif
    return ERR_OK;
}

static void tud_network_lwip_init() {
    struct netif *netif = &netif_data;

    /* the lwip virtual MAC address must be different from the host's; to ensure this, we toggle the LSbit */
    netif->hwaddr_len = sizeof(tud_network_mac_address);
    memcpy(netif->hwaddr, tud_network_mac_address, sizeof(tud_network_mac_address));
    netif->hwaddr[5] ^= 0x01;

    netif = netif_add_noaddr(netif, NULL, tud_network_lwip_netif_init, tcpip_input);
    #if LWIP_IPV6
    netif_create_ip6_linklocal_address(netif, 1);
    #endif
}

void tud_network_init(void) {
    tx_queue_mutex = xSemaphoreCreateMutex();
    LOCK_TCPIP_CORE();
    tud_network_lwip_init();
    UNLOCK_TCPIP_CORE();
}

void lwip_network_set_link(void *arg) {
    bool up = (intptr_t)arg;
    struct netif *netif = &netif_data;

    if (up) {
        netif_set_link_up(netif);
    } else {
        netif_set_link_down(netif);
    }
}

void tud_network_set_link(bool up) {
    LOCK_TCPIP_CORE();
    lwip_network_set_link((void *)up);
    UNLOCK_TCPIP_CORE();
}

bool tud_network_recv_cb(const uint8_t *src, uint16_t size) {
    if (size) {
        struct netif *netif = &netif_data;
        struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
        if (p) {
            pbuf_take(p, src, size);
            netif->input(p, netif);
        }
        tud_network_recv_renew();
    }
    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg) {
    struct pbuf *p = (struct pbuf *)ref;
    return pbuf_copy_partial(p, dst, p->tot_len, 0);
}

void tud_network_init_cb(void) {
    if (!xSemaphoreTake(tx_queue_mutex, portMAX_DELAY)) {
        return;
    }

    if (tx_queue_head) {
        pbuf_free(tx_queue_head);
        tx_queue_head = NULL;
        tx_queue_tail = &tx_queue_head;
    }

    xSemaphoreGive(tx_queue_mutex);
}

void tud_network_link_state_cb(bool state) {
}
#endif
