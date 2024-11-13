// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "pico/unique_id.h"

#include "tinyusb/tusb_config.h"

#define TUSB_ENV_DEVICE 0x0100
#define TUSB_ENV_CONFIG 0x0200
#define TUSB_ENV_STRING 0x0300


const void *flash_env_get(int key, size_t *len) {
    return NULL;
}

void tusb_config_load(tusb_config_t *config) {
    size_t len;
    config->device = (void *)flash_env_get(TUSB_ENV_DEVICE, &len);
    for (size_t i = 0; i < TUSB_CONFIG_MAX_CFGS; i++) {
        config->configs[i] = (void *)flash_env_get(TUSB_ENV_CONFIG + i, &len);
    }
    for (size_t i = 0; i < TUSB_CONFIG_MAX_STRS; i++) {
        config->strings[i] = (void *)flash_env_get(TUSB_ENV_STRING + i, &len);
    }
}

bool tusb_config_save(const tusb_config_t *config) {
    #if 0
    struct flash_env *env = flash_env_open();
    if (!env) {
        return false;
    }

    flash_env_set(env, TUSB_ENV_DEVICE, config->device, sizeof(tusb_desc_device_t));

    for (size_t i = 0; i < TUSB_CONFIG_MAX_CFGS; i++) {
        const tusb_desc_configuration_t *cfg = config->configs[i];
        flash_env_set(env, TUSB_ENV_CONFIG + i, cfg, cfg ? cfg->wTotalLength : 0);
    }

    for (size_t i = 0; i < TUSB_CONFIG_MAX_STRS; i++) {
        const tusb_desc_string_t *str = config->strings[i];
        flash_env_set(env, TUSB_ENV_STRING + i, str, str ? str->bLength : 0);
    }

    bool connected = tud_connected();
    tud_disconnect();

    flash_env_close(env);

    if (connected) {
        tud_connect();
    }
    return true;
    #else
    return false;
    #endif
}

#define USBD_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN)
#define USBD_MAX_POWER_MA (250)

#define USBD_ITF_CDC       (0) // needs 2 interfaces
#define USBD_ITF_MSC       (2) // needs 2 interfaces
#define USBD_ITF_MAX       (3)

#define USBD_CDC_EP_CMD (0x81)
#define USBD_CDC_EP_OUT (0x02)
#define USBD_CDC_EP_IN (0x82)
#define USBD_CDC_CMD_MAX_SIZE (8)
#define USBD_CDC_IN_OUT_MAX_SIZE (64)

#define USBD_MSC_EP_OUT (0x03)
#define USBD_MSC_EP_IN (0x83)
#define USBD_MSC_EP_SIZE (64)

#define USBD_STR_0 (0x00)
#define USBD_STR_MANUF (0x01)
#define USBD_STR_PRODUCT (0x02)
#define USBD_STR_SERIAL (0x03)
#define USBD_STR_CDC (0x04)
#define USBD_STR_MSC (0x05)

static const tusb_desc_device_t usbd_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x2E8A, // Raspberry Pi
    .idProduct = 0x4003,
    .bcdDevice = 0x0100,
    .iManufacturer = USBD_STR_MANUF,
    .iProduct = USBD_STR_PRODUCT,
    .iSerialNumber = USBD_STR_SERIAL,
    .bNumConfigurations = 1,
};

static const uint8_t usbd_desc_cfg[USBD_DESC_LEN] = {
    TUD_CONFIG_DESCRIPTOR(1, USBD_ITF_MAX, USBD_STR_0, USBD_DESC_LEN, 0, USBD_MAX_POWER_MA),

    TUD_CDC_DESCRIPTOR(USBD_ITF_CDC, USBD_STR_CDC, USBD_CDC_EP_CMD, USBD_CDC_CMD_MAX_SIZE, USBD_CDC_EP_OUT, USBD_CDC_EP_IN, USBD_CDC_IN_OUT_MAX_SIZE),

    TUD_MSC_DESCRIPTOR(USBD_ITF_MSC, USBD_STR_MSC, USBD_MSC_EP_OUT, USBD_MSC_EP_IN, USBD_MSC_EP_SIZE),
};

static const uint16_t usbd_desc_str0[] = { 4 | (TUSB_DESC_STRING << 8), 0x0409 };
static const uint16_t usbd_desc_str_manuf[] = { 26 | (TUSB_DESC_STRING << 8), 'R', 'a', 's', 'p', 'b', 'e', 'r', 'r', 'y', ' ', 'P', 'i' };
static const uint16_t usbd_desc_str_product[] = { 10 | (TUSB_DESC_STRING << 8), 'P', 'i', 'c', 'o' };
static uint16_t usbd_desc_str_serial[] = { 34 | (TUSB_DESC_STRING << 8), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static const uint16_t usbd_desc_str_cdc[] = { 20 | (TUSB_DESC_STRING << 8), 'B', 'o', 'a', 'r', 'd', ' ', 'C', 'D', 'C' };
static const uint16_t usbd_desc_str_msc[] = { 20 | (TUSB_DESC_STRING << 8), 'B', 'o', 'a', 'r', 'd', ' ', 'M', 'S', 'C' };

const uint8_t *tud_descriptor_device_cb(void) {
    size_t len;
    const void *device = flash_env_get(TUSB_ENV_DEVICE, &len);
    return device ? device : (uint8_t *)&usbd_desc_device;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    size_t len;
    if (flash_env_get(TUSB_ENV_DEVICE, &len)) {
        return flash_env_get(TUSB_ENV_CONFIG + index, &len);
    }
    return usbd_desc_cfg;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    size_t len;
    if (flash_env_get(TUSB_ENV_DEVICE, &len)) {
        return flash_env_get(TUSB_ENV_STRING + index, &len);
    }
    if (index == USBD_STR_SERIAL) {
        char usbd_serial_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
        pico_get_unique_board_id_string(usbd_serial_str, sizeof(usbd_serial_str));
        for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
            usbd_desc_str_serial[1 + i] = usbd_serial_str[i];
        }
    }
    switch (index) {
        case USBD_STR_0:
            return usbd_desc_str0;
        case USBD_STR_MANUF:
            return usbd_desc_str_manuf;
        case USBD_STR_PRODUCT:
            return usbd_desc_str_product;
        case USBD_STR_SERIAL:
            return usbd_desc_str_serial;
        case USBD_STR_CDC:
            return usbd_desc_str_cdc;
        case USBD_STR_MSC:
            return usbd_desc_str_msc;
        default:
            return NULL;
    }
}
