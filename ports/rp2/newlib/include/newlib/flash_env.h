// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include "newlib/flash.h"


struct flash_env_item {
    uint key : 16;
    uint len : 12;
    uint check : 4;
    char value[];
};

struct flash_env {
    struct flash_env_item head;
    char buffer[FLASH_SECTOR_SIZE - 4];
};

const void *flash_env_get(int key, size_t *len);

struct flash_env *flash_env_open(void);

int flash_env_set(struct flash_env *env, int key, const void *value, size_t len);

void flash_env_close(struct flash_env *env);
