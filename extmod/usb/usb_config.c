// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <string.h>

#include "tusb.h"
#include "tinyusb/tusb_config.h"

#include "shared/tinyusb/mp_usbd.h"
#include "py/runtime.h"
#include "./usb_config.h"

#define USBD_MAX_DESC_LEN 256

#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) | _PID_MAP(ECM_RNDIS, 5) | _PID_MAP(NCM, 5) )


typedef struct {
    mp_obj_base_t base;
    tusb_config_t tusb_config;
    size_t cfg_idx;
    size_t ep_idx;
    size_t string_idx;
} usb_config_obj_t;

STATIC const tusb_desc_string_t usb_string_0 = { 4, TUSB_DESC_STRING, { 0x0409 } };

STATIC uint8_t usb_config_str(usb_config_obj_t *self, mp_obj_t py_str, const char *c_str) {
    if ((py_str != MP_OBJ_NULL) && (py_str != mp_const_none)) {
        c_str = mp_obj_str_get_str(py_str);
    }
    if ((c_str == NULL) || (strlen(c_str) == 0)) {
        return 0;
    }
    
    size_t idx = self->string_idx;
    assert(idx < TUSB_CONFIG_MAX_STRS);
    tusb_desc_string_t *desc = m_malloc(USBD_MAX_DESC_LEN);
    desc->bLength = 2;
    desc->bDescriptorType = TUSB_DESC_STRING;
    size_t i = 0;
    const byte *b_str = (const byte *)c_str;
    size_t len = strlen(c_str);
    for (const byte *c = b_str; (c < b_str + len) && (i < 126); c = utf8_next_char(c)) {
        unichar u = utf8_get_char(c);
        desc->unicode_string[i++] = u < 0x10000 ? u : 0xFFFD;
        desc->bLength += 2;
    }

    self->tusb_config.strings[idx] = desc;
    self->string_idx++;
    return idx;
}

STATIC mp_obj_t usb_config_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    usb_config_obj_t *self = m_new_obj(usb_config_obj_t);
    memset(self, 0, sizeof(usb_config_obj_t));
    self->base.type = type;
    return MP_OBJ_FROM_PTR(self);
}

#if CFG_TUH_ENABLED
STATIC mp_obj_t usb_config_host(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    // self->tusb_config.host = true;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_host_obj, 1, usb_config_host);
#endif

#if CFG_TUD_ENABLED
STATIC mp_obj_t usb_config_device(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);

    self->tusb_config.device = NULL;
    for (size_t i = 0; i < TUSB_CONFIG_MAX_CFGS; i++) {
        self->tusb_config.configs[i] = NULL;
    }
    for (size_t i = 0; i < TUSB_CONFIG_MAX_STRS; i++) {
        self->tusb_config.strings[i] = NULL;
    }
    self->tusb_config.strings[0] = (tusb_desc_string_t *)&usb_string_0;
    self->string_idx = 1; 

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_vid), MP_MAP_LOOKUP);
    uint16_t vid = elem ? mp_obj_get_int(elem->value) : MICROPY_HW_USB_VID;

    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_pid), MP_MAP_LOOKUP);
    uint16_t pid = elem ? mp_obj_get_int(elem->value) : USB_PID;

    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_device), MP_MAP_LOOKUP);
    uint16_t device = elem ? mp_obj_get_int(elem->value) : 0x0100;

    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_manufacturer), MP_MAP_LOOKUP);
    uint8_t manufacturer_idx = usb_config_str(self, elem ? elem->value : MP_OBJ_NULL, MICROPY_HW_USB_MANUFACTURER_STRING);

    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_product), MP_MAP_LOOKUP);
    uint8_t product_idx = usb_config_str(self, elem ? elem->value : MP_OBJ_NULL, MICROPY_HW_USB_PRODUCT_FS_STRING);

    char serial[MICROPY_HW_USB_DESC_STR_MAX];
    mp_usbd_port_get_serial_number(serial);
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_serial), MP_MAP_LOOKUP);
    uint8_t serial_idx = usb_config_str(self, elem ? elem->value : MP_OBJ_NULL, serial);  

    tusb_desc_device_t *desc = m_malloc(sizeof(tusb_desc_device_t));
    desc->bLength = sizeof(tusb_desc_device_t);
    desc->bDescriptorType = TUSB_DESC_DEVICE;
    desc->bcdUSB = 0x0200;
    desc->bDeviceClass = TUSB_CLASS_MISC;
    desc->bDeviceSubClass = MISC_SUBCLASS_COMMON;
    desc->bDeviceProtocol = MISC_PROTOCOL_IAD;
    desc->bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE;
    desc->idVendor = vid;
    desc->idProduct = pid;
    desc->bcdDevice = device;
    desc->iManufacturer = manufacturer_idx;
    desc->iProduct = product_idx;
    desc->iSerialNumber = serial_idx;
    desc->bNumConfigurations = 0;

    self->tusb_config.device = desc;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_device_obj, 1, usb_config_device);

STATIC mp_obj_t usb_config_configuration(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    uint8_t str_idx = 0;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    if (elem) {
        str_idx = usb_config_str(self, elem->value, NULL);
    }

    uint8_t attribute = 0;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_attribute), MP_MAP_LOOKUP);
    if (elem) {
        attribute = mp_obj_get_int(elem->value);
    }

    uint8_t power = USBD_MAX_POWER_MA;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_power_ma), MP_MAP_LOOKUP);
    if (elem) {
        power = MIN((mp_uint_t)mp_obj_get_int(elem->value) / 2, USBD_MAX_POWER_MA);
    }

    size_t idx = self->tusb_config.device->bNumConfigurations;
    assert(idx < TUSB_CONFIG_MAX_CFGS);
    tusb_desc_configuration_t *desc = m_malloc(USBD_MAX_DESC_LEN);
    desc->bLength = TUD_CONFIG_DESC_LEN;
    desc->bDescriptorType = TUSB_DESC_CONFIGURATION;
    desc->wTotalLength = desc->bLength;
    desc->bNumInterfaces = 0;
    desc->bConfigurationValue = idx + 1;
    desc->iConfiguration = str_idx;
    desc->bmAttributes = 0x80 | (attribute & 0x60);
    desc->bMaxPower = power;

    self->tusb_config.configs[idx] = desc;
    self->tusb_config.device->bNumConfigurations++;
    self->cfg_idx = idx;
    self->ep_idx = 1;

    return MP_OBJ_NEW_SMALL_INT(idx);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_configuration_obj, 1, usb_config_configuration);

STATIC tusb_desc_configuration_t **usb_config_cfg_get(usb_config_obj_t *self) {
    assert(self->tusb_config.device);
    assert(self->cfg_idx < self->tusb_config.device->bNumConfigurations);
    return &self->tusb_config.configs[self->cfg_idx];
}

STATIC tusb_desc_configuration_t *usb_config_cfg_append(usb_config_obj_t *self, uint8_t *buf, size_t len) {
    tusb_desc_configuration_t *desc = *usb_config_cfg_get(self);
    assert(desc->wTotalLength + len <= 0xffff);
    desc = m_realloc(desc, (desc->wTotalLength + len + 255) & ~255);
    uint8_t *p = (uint8_t *)desc + desc->wTotalLength;
    memcpy(p, buf, len);
    desc->wTotalLength += len;
    *usb_config_cfg_get(self) = desc;
    return desc;
}

#if CFG_TUD_CDC
STATIC mp_obj_t usb_config_cdc(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device || (self->cfg_idx >= self->tusb_config.device->bNumConfigurations)) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
    
    tusb_desc_configuration_t *cfg_desc = *usb_config_cfg_get(self);
    size_t itf = cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_CDC_DESCRIPTOR(itf, str_idx, 0x80 | ep_idx, 8, ep_idx + 1, 0x80 | (ep_idx + 1), 64)
    };
    cfg_desc = usb_config_cfg_append(self, desc, sizeof(desc));
    cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_cdc_obj, 1, usb_config_cdc);
#endif

#if CFG_TUD_MSC
STATIC mp_obj_t usb_config_msc(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device || (self->cfg_idx >= self->tusb_config.device->bNumConfigurations)) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
    
    tusb_desc_configuration_t *cfg_desc = *usb_config_cfg_get(self);
    size_t itf = cfg_desc->bNumInterfaces;
    assert(itf < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_MSC_DESCRIPTOR(itf, str_idx, ep_idx, 0x80 | ep_idx, 64)
    };
    cfg_desc = usb_config_cfg_append(self, desc, sizeof(desc));
    cfg_desc->bNumInterfaces += 1;
    self->ep_idx += 1;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_msc_obj, 1, usb_config_msc);
#endif

#if CFG_TUD_AUDIO && CFG_TUD_AUDIO_ENABLE_EP_OUT
STATIC mp_obj_t usb_config_audio_speaker(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device || (self->cfg_idx >= self->tusb_config.device->bNumConfigurations)) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
    
    tusb_desc_configuration_t *cfg_desc = *usb_config_cfg_get(self);
    size_t itf = cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t cdc_desc[] = {
        TUD_AUDIO_SPEAKER_MONO_FB_DESCRIPTOR(itf, str_idx, 2 /*_nBytesPerSample*/, 16 /*_nBitsUsedPerSample*/, ep_idx, 64, 0x80 | (ep_idx + 1))
    };
    cfg_desc = usb_config_cfg_append(self, cdc_desc, sizeof(cdc_desc));
    cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_audio_speaker_obj, 1, usb_config_audio_speaker);
#endif

#if CFG_TUD_AUDIO && CFG_TUD_AUDIO_ENABLE_EP_IN
STATIC mp_obj_t usb_config_audio_mic(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device || (self->cfg_idx >= self->tusb_config.device->bNumConfigurations)) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);

    tusb_desc_configuration_t *cfg_desc = *usb_config_cfg_get(self);
    size_t itf = cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(itf, str_idx, /*_nBytesPerSample*/ CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, /*_nBitsUsedPerSample*/ CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX*8, /*_epin*/ 0x80 | ep_idx, /*_epsize*/ CFG_TUD_AUDIO_EP_SZ_IN)
    };
    cfg_desc = usb_config_cfg_append(self, desc, sizeof(desc));
    cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 1;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_audio_mic_obj, 1, usb_config_audio_mic);
#endif

#if CFG_TUD_ECM_RNDIS
STATIC mp_obj_t usb_config_net_ecm(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device || (self->cfg_idx >= self->tusb_config.device->bNumConfigurations)) {
        mp_raise_ValueError(NULL);
    }

    uint8_t mac_idx = usb_config_str(self, args[1], NULL);

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
  
    tusb_desc_configuration_t *cfg_desc = *usb_config_cfg_get(self);
    size_t itf = cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_CDC_ECM_DESCRIPTOR(itf, str_idx, mac_idx, 0x80 | ep_idx, 64, ep_idx + 1, 0x80 | (ep_idx + 1), CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU)
    };
    cfg_desc = usb_config_cfg_append(self, desc, sizeof(desc));
    cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_net_ecm_obj, 2, usb_config_net_ecm);

STATIC mp_obj_t usb_config_net_rndis(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device || (self->cfg_idx >= self->tusb_config.device->bNumConfigurations)) {
        mp_raise_ValueError(NULL);
    }

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);

    tusb_desc_configuration_t *cfg_desc = *usb_config_cfg_get(self);
    size_t itf = cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_RNDIS_DESCRIPTOR(itf, str_idx, 0x80 | ep_idx, 8, ep_idx + 1, 0x80 | (ep_idx + 1), CFG_TUD_NET_ENDPOINT_SIZE)
    };
    cfg_desc = usb_config_cfg_append(self, desc, sizeof(desc));
    cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_net_rndis_obj, 1, usb_config_net_rndis);
#endif

#if CFG_TUD_NCM
STATIC mp_obj_t usb_config_net_ncm(size_t n_args, const mp_obj_t *args, mp_map_t *kws) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(args[0]);
    if (!self->tusb_config.device || (self->cfg_idx >= self->tusb_config.device->bNumConfigurations)) {
        mp_raise_ValueError(NULL);
    }

    uint8_t mac_idx = usb_config_str(self, args[1], NULL);

    mp_map_elem_t *elem;
    elem = mp_map_lookup(kws, MP_OBJ_NEW_QSTR(MP_QSTR_str), MP_MAP_LOOKUP);
    uint8_t str_idx = usb_config_str(self, elem ? elem->value : NULL, NULL);
  
    tusb_desc_configuration_t *cfg_desc = *usb_config_cfg_get(self);
    size_t itf = cfg_desc->bNumInterfaces;
    assert(itf + 1 < CFG_TUD_INTERFACE_MAX);
    size_t ep_idx = self->ep_idx;
    uint8_t desc[] = {
        TUD_CDC_NCM_DESCRIPTOR(itf, str_idx, mac_idx, 0x80 | ep_idx, 64, ep_idx + 1, 0x80 | (ep_idx + 1), CFG_TUD_NET_ENDPOINT_SIZE, CFG_TUD_NET_MTU)
    };
    cfg_desc = usb_config_cfg_append(self, desc, sizeof(desc));
    cfg_desc->bNumInterfaces += 2;
    self->ep_idx += 2;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(usb_config_net_ncm_obj, 2, usb_config_net_ncm);
#endif
#endif

STATIC void usb_config_print_bytes(const mp_print_t *print, uint8_t *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        mp_printf(print, "0x%02x, ", (int)bytes[i]);
    }
}

STATIC void usb_config_print_tusb_config(const mp_print_t *print, const tusb_config_t *tusb_config) {
    mp_printf(print, "uint8_t device[] = { ");
    usb_config_print_bytes(print, (uint8_t *)tusb_config->device, sizeof(tusb_desc_device_t));
    mp_printf(print, "};\n");

    mp_printf(print, "uint8_t configs[][] = {\n");
    for (size_t i = 0; i < tusb_config->device->bNumConfigurations; i++) {
        const tusb_desc_configuration_t *cfg = tusb_config->configs[i];
        mp_printf(print, "    { ");
        usb_config_print_bytes(print, (uint8_t *)cfg, cfg->wTotalLength);
        mp_printf(print, "},\n");
    }
    mp_printf(print, "};\n");

    mp_printf(print, "uint16_t strings[][] = {\n");
    for (size_t i = 0; i < TUSB_CONFIG_MAX_STRS; i++) {
        const tusb_desc_string_t *str = tusb_config->strings[i];
        if (!str) {
            break;
        }
        mp_printf(print, "    { ");
        for (size_t j = 0; j < str->bLength / sizeof(uint16_t); j++) {
            mp_printf(print, "0x%04x, ", (int)(((uint16_t *)str)[j]));
        }
        mp_printf(print, "},\n");
    }
    mp_printf(print, "};\n");    
}

STATIC void usb_config_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(self_in);

    if (kind == PRINT_REPR) {
        const mp_obj_type_t *type = mp_obj_get_type(self_in);
        mp_printf(print, "<%q>", type->name);
        return;
    }

    if (!self->tusb_config.device) {
        return;
    }

    usb_config_print_tusb_config(print, &self->tusb_config);
}

STATIC mp_obj_t usb_config_exists(void) {
    tusb_config_t config;
    tusb_config_load(&config);
    return mp_obj_new_bool(!!config.device);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(usb_config_exists_fun, usb_config_exists);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(usb_config_exists_obj, MP_ROM_PTR(&usb_config_exists_fun));

STATIC mp_obj_t usb_config_delete(void) {
    tusb_config_delete();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(usb_config_delete_fun, usb_config_delete);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(usb_config_delete_obj, MP_ROM_PTR(&usb_config_delete_fun));

STATIC mp_obj_t usb_config_save(mp_obj_t self_in) {
    usb_config_obj_t *self = MP_OBJ_TO_PTR(self_in);
    tusb_config_save(&self->tusb_config);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(usb_config_save_obj, usb_config_save);

// STATIC mp_obj_t usb_config_load(void) {
//     mp_obj_t self_in = usb_config_make_new(&type, 0, 0, NULL);
//     usb_config_obj_t *self = MP_OBJ_TO_PTR(self_in);
//     tusb_config_t *config = &self->tusb_config;
//     tusb_config_load(config);
//     // TODO copy pointers from flash to RAM
//     return self_in;
// }
// STATIC MP_DEFINE_CONST_FUN_OBJ_0(usb_config_load_fun, usb_config_load);
// STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(usb_config_load_obj, usb_config_load_fun);

STATIC const mp_rom_map_elem_t usb_config_locals_dict_table[] = {
#if CFG_TUH_ENABLED    
    { MP_ROM_QSTR(MP_QSTR_host),          MP_ROM_PTR(&usb_config_host_obj) },
#endif
#if CFG_TUD_ENABLED    
    { MP_ROM_QSTR(MP_QSTR_device),          MP_ROM_PTR(&usb_config_device_obj) },
    { MP_ROM_QSTR(MP_QSTR_configuration),   MP_ROM_PTR(&usb_config_configuration_obj) },
    #if CFG_TUD_CDC
    { MP_ROM_QSTR(MP_QSTR_cdc),             MP_ROM_PTR(&usb_config_cdc_obj) },
    #endif
    #if CFG_TUD_MSC
    { MP_ROM_QSTR(MP_QSTR_msc),             MP_ROM_PTR(&usb_config_msc_obj) },
    #endif
    #if CFG_TUD_AUDIO && CFG_TUD_AUDIO_ENABLE_EP_OUT
    { MP_ROM_QSTR(MP_QSTR_audio_speaker),   MP_ROM_PTR(&usb_config_audio_speaker_obj) },
    #endif    
    #if CFG_TUD_AUDIO && CFG_TUD_AUDIO_ENABLE_EP_IN
    { MP_ROM_QSTR(MP_QSTR_audio_mic),       MP_ROM_PTR(&usb_config_audio_mic_obj) },
    #endif
    #if CFG_TUD_ECM_RNDIS
    { MP_ROM_QSTR(MP_QSTR_net_ecm),         MP_ROM_PTR(&usb_config_net_ecm_obj) },
    { MP_ROM_QSTR(MP_QSTR_net_rndis),       MP_ROM_PTR(&usb_config_net_rndis_obj) },
    #endif
    #if CFG_TUD_NCM
    { MP_ROM_QSTR(MP_QSTR_net_ncm),         MP_ROM_PTR(&usb_config_net_ncm_obj) },
    #endif
#endif
    { MP_ROM_QSTR(MP_QSTR_exists),          MP_ROM_PTR(&usb_config_exists_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete),          MP_ROM_PTR(&usb_config_delete_obj) },
    { MP_ROM_QSTR(MP_QSTR_save),            MP_ROM_PTR(&usb_config_save_obj) },
    // { MP_ROM_QSTR(MP_QSTR_load),            MP_ROM_PTR(&usb_config_load_obj) },
};
STATIC MP_DEFINE_CONST_DICT(usb_config_locals_dict, usb_config_locals_dict_table);

MP_DEFINE_CONST_OBJ_TYPE(
    usb_config_type,
    MP_QSTR_UsbConfig,
    MP_TYPE_FLAG_NONE,
    make_new, usb_config_make_new,
    print, usb_config_print,
    locals_dict, &usb_config_locals_dict
    );