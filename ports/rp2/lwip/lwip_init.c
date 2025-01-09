// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <string.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "semphr.h"

#include "lwip/tcpip.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/sntp.h"


#if LWIP_MDNS_RESPONDER
static void lwip_helper_netif_cb(struct netif *netif, netif_nsc_reason_t reason, const netif_ext_callback_args_t *args) {
    if (reason & LWIP_NSC_NETIF_ADDED) {
        char hostname[MDNS_LABEL_MAXLEN + 1];
        if ((gethostname(hostname, MDNS_LABEL_MAXLEN) >= 0) && strnlen(hostname, MDNS_LABEL_MAXLEN)) {
            mdns_resp_add_netif(netif, hostname);
        }
    }
    if (reason & LWIP_NSC_NETIF_REMOVED) {
        mdns_resp_remove_netif(netif);
    }
}
#endif

static void lwip_init_cb(void *arg) {
    SemaphoreHandle_t init_sem = arg;
    xSemaphoreGive(init_sem);

    #if LWIP_MDNS_RESPONDER
    mdns_resp_init();
    NETIF_DECLARE_EXT_CALLBACK(netif_callback);
    netif_add_ext_callback(&netif_callback, lwip_helper_netif_cb);
    #endif

    sntp_servermode_dhcp(1);
    sntp_init();
}

void lwip_helper_init(void) {
    SemaphoreHandle_t init_sem = xSemaphoreCreateBinary();
    tcpip_init(lwip_init_cb, init_sem);
    xSemaphoreTake(init_sem, portMAX_DELAY);
    vSemaphoreDelete(init_sem);
}
