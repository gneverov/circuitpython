// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "tinyusb/net_device_lwip.h"

void tud_mount_cb(void) {
    #if CFG_TUD_ECM_RNDIS || CFG_TUD_NCM
    tud_network_set_link(true);
    #endif
}

void tud_umount_cb(void) {
    #if CFG_TUD_ECM_RNDIS || CFG_TUD_NCM
    tud_network_set_link(false);
    #endif
}
