// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once

#include "tusb.h"

#define TUSB_CONFIG_MAX_CFGS 2
#define TUSB_CONFIG_MAX_STRS 16

// #define TUSB_CONFIG_MAX_DESC_LEN 256

typedef struct {
    tusb_desc_device_t *device;
    tusb_desc_configuration_t *configs[TUSB_CONFIG_MAX_CFGS];
    tusb_desc_string_t *strings[TUSB_CONFIG_MAX_STRS];
} tusb_config_t;

void tusb_config_load(tusb_config_t *config);

void tusb_config_delete(void);

bool tusb_config_save(const tusb_config_t *config);
