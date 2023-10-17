// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "tusb.h"

#define TUSB_CONFIG_MAX_CFGS 2
#define TUSB_CONFIG_MAX_STRS 16

// #define TUSB_CONFIG_MAX_DESC_LEN 256

typedef struct {
    uint16_t magic;
    bool host;
    uint8_t cdc_itf;
    bool disconnect;

    tusb_desc_device_t *device;
    tusb_desc_configuration_t *configs[TUSB_CONFIG_MAX_CFGS];
    tusb_desc_string_t *strings[TUSB_CONFIG_MAX_STRS];
    uint8_t heap[];
} tusb_config_t;

void tusb_config_init(tusb_config_t *tusb_config);

const tusb_config_t *tusb_config_get(void);

void tusb_config_delete(void);

size_t tusb_config_save(const tusb_config_t *tusb_config);
