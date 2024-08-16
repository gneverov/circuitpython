// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "FreeRTOS.h"
#include "semphr.h"

#include "lwip/tcpip.h"
#include "lwip/apps/mdns.h"
#include "lwip/apps/sntp.h"


static void lwip_init_cb(void *arg) {
    SemaphoreHandle_t init_sem = arg;
    xSemaphoreGive(init_sem);

    #if LWIP_MDNS_RESPONDER
    mdns_resp_init();
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
