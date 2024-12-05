// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include "tinyusb/tusb_config.h"

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
#define USBD_STR_SERIAL USBD_STR_0
#define USBD_STR_CDC (0x03)
#define USBD_STR_MSC (0x04)


__attribute__((visibility("hidden")))
const tusb_config_t tusb_default_config = {
    .device = {
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
    },
    .configs = {
        TUD_CONFIG_DESCRIPTOR(1, USBD_ITF_MAX, USBD_STR_0, USBD_DESC_LEN, 0, USBD_MAX_POWER_MA),
        TUD_CDC_DESCRIPTOR(USBD_ITF_CDC, USBD_STR_CDC, USBD_CDC_EP_CMD, USBD_CDC_CMD_MAX_SIZE, USBD_CDC_EP_OUT, USBD_CDC_EP_IN, USBD_CDC_IN_OUT_MAX_SIZE),
        TUD_MSC_DESCRIPTOR(USBD_ITF_MSC, USBD_STR_MSC, USBD_MSC_EP_OUT, USBD_MSC_EP_IN, USBD_MSC_EP_SIZE),
        0,
    },
    .strings = {
        4 | (TUSB_DESC_STRING << 8), 0x0409,
        26 | (TUSB_DESC_STRING << 8), 'R', 'a', 's', 'p', 'b', 'e', 'r', 'r', 'y', ' ', 'P', 'i',
        10 | (TUSB_DESC_STRING << 8), 'P', 'i', 'c', 'o',
        20 | (TUSB_DESC_STRING << 8), 'B', 'o', 'a', 'r', 'd', ' ', 'C', 'D', 'C',
        20 | (TUSB_DESC_STRING << 8), 'B', 'o', 'a', 'r', 'd', ' ', 'M', 'S', 'C',
        0,
    },
    .msc_vendor_id = MICROPY_HW_USB_MSC_INQUIRY_VENDOR_STRING,
    .msc_product_id = MICROPY_HW_USB_MSC_INQUIRY_PRODUCT_STRING,
    .msc_product_rev = MICROPY_HW_USB_MSC_INQUIRY_REVISION_STRING,
};
