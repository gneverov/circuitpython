// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "tinyusb/net_device_lwip.h"

void tud_mount_cb(void) {
    tud_network_set_link(true);
}

void tud_umount_cb(void) {
    tud_network_set_link(false);
}
