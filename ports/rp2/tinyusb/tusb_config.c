// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "tinyusb/tusb_config.h"

#include <malloc.h>
#include <memory.h>

#include "FreeRTOS.h"
#include "task.h"

#include "hardware/flash.h"

#define TUSB_CONFIG_MAGIC 0x4e47


__attribute__((section(".usb_config")))
static const volatile tusb_config_t tusb_config_flash;

void tusb_config_init(tusb_config_t *tusb_config) {
    memset(tusb_config, 0, sizeof(tusb_config_t));
    tusb_config->magic = TUSB_CONFIG_MAGIC;
    tusb_config->cdc_itf = -1;
}

const tusb_config_t *tusb_config_get(void) {
    const tusb_config_t *tusb_config = (tusb_config_t *)&tusb_config_flash;
    return (tusb_config->magic == TUSB_CONFIG_MAGIC) ? tusb_config : NULL;
}

void tusb_config_delete(void) {
    tud_disconnect();

    taskENTER_CRITICAL();
    uint32_t tusb_config_flash_offset = ((uintptr_t)&tusb_config_flash) - XIP_BASE;
    flash_range_erase(tusb_config_flash_offset, FLASH_SECTOR_SIZE);
    taskEXIT_CRITICAL();
}

size_t tusb_config_save(const tusb_config_t *ram_config) {
    tusb_config_t *flash_config = malloc(FLASH_SECTOR_SIZE);
    if (!flash_config) {
        return 0;
    }

    tusb_config_init(flash_config);
    *flash_config = *ram_config;

    uint8_t *heap = flash_config->heap;
    ptrdiff_t diff = (uint8_t *)&tusb_config_flash - (uint8_t *)flash_config;

    // flash_config->device = NULL;
    tusb_desc_device_t *dev = ram_config->device;
    if (dev) {
        flash_config->device = (tusb_desc_device_t *)(heap + diff);
        memcpy(heap, dev, dev->bLength);
        heap += dev->bLength;
    }

    for (size_t i = 0; i < TUSB_CONFIG_MAX_CFGS; i++) {
        // flash_config->configs[i] = NULL;
        tusb_desc_configuration_t *cfg = ram_config->configs[i];
        if (cfg) {
            flash_config->configs[i] = (tusb_desc_configuration_t *)(heap + diff);
            memcpy(heap, cfg, cfg->wTotalLength);
            heap += cfg->wTotalLength;
        }
    }

    for (size_t i = 0; i < TUSB_CONFIG_MAX_STRS; i++) {
        // flash_config->strings[i] = NULL;
        tusb_desc_string_t *str = ram_config->strings[i];
        if (str) {
            flash_config->strings[i] = (tusb_desc_string_t *)(heap + diff);
            memcpy(heap, str, str->bLength);
            heap += str->bLength;
        }
    }

    bool connected = tud_connected();
    tud_disconnect();

    taskENTER_CRITICAL();
    uint32_t tusb_config_flash_offset = ((uintptr_t)&tusb_config_flash) - XIP_BASE;
    flash_range_erase(tusb_config_flash_offset, FLASH_SECTOR_SIZE);
    flash_range_program(tusb_config_flash_offset, (uint8_t *)flash_config, FLASH_SECTOR_SIZE);
    taskEXIT_CRITICAL();

    free(flash_config);
    if (connected) {
        tud_connect();
    }

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wuse-after-free"
    return heap - (uint8_t *)flash_config;
    #pragma GCC diagnostic pop
}

const uint8_t *tud_descriptor_device_cb(void) {
    const tusb_config_t *tusb_config = tusb_config_get();
    return tusb_config ? (uint8_t *)tusb_config->device : NULL;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    const tusb_config_t *tusb_config = tusb_config_get();
    return (tusb_config && (index < TUSB_CONFIG_MAX_CFGS)) ? (uint8_t *)tusb_config->configs[index] : NULL;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    const tusb_config_t *tusb_config = tusb_config_get();
    return (tusb_config && (index < TUSB_CONFIG_MAX_STRS)) ? (uint16_t *)tusb_config->strings[index] : NULL;
}
