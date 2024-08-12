// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "FreeRTOS.h"
#include "semphr.h"

#include "lwip/tcpip.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/sntp.h"

static SemaphoreHandle_t lwip_inited;

static void lwip_init_cb(void *arg) {
    xSemaphoreGive(lwip_inited);

    #if LWIP_MDNS_RESPONDER
    mdns_resp_init();
    #endif

    sntp_servermode_dhcp(1);
    sntp_init();
}

void lwip_helper_init(void) {
    lwip_inited = xSemaphoreCreateBinary();
    tcpip_init(lwip_init_cb, NULL);
}

void lwip_wait(void) {
    xSemaphoreTake(lwip_inited, portMAX_DELAY);
}
