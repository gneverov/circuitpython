// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <errno.h>
#include <malloc.h>
#include <memory.h>

#include "FreeRTOS.h"
#include "task.h"

#include "newlib/flash_env.h"


static_assert(sizeof(struct flash_env) == FLASH_SECTOR_SIZE);

__attribute__((section(".flash_env")))
static struct flash_env flash_env;

static inline struct flash_env_item *flash_env_next(const struct flash_env_item *item) {
    return (struct flash_env_item *)(((uintptr_t)item + item->len + __alignof(struct flash_env) - 1) & ~(__alignof(struct flash_env) - 1));
}

const void *flash_env_get(int key, size_t *len) {
    uint i = 0;
    struct flash_env_item *item = &flash_env.head;
    while (item->len && (item->check == (i++ & 15))) {
        if (item->key == key) {
            *len = item->len - sizeof(struct flash_env_item);
            return item->value;
        }
        item = flash_env_next(item);
    }
    return NULL;
}

struct flash_env *flash_env_open(void) {
    struct flash_env *env = malloc(sizeof(struct flash_env));
    if (!env) {
        errno = ENOMEM;
        return NULL;
    }
    if (flash_env.head.check == 0) {
        *env = flash_env;
    } else {
        memset(env, 0, sizeof(struct flash_env));
    }
    return env;
}

int flash_env_set(struct flash_env *env, int key, const void *value, size_t len) {
    struct flash_env_item *src = &env->head;
    struct flash_env_item *dst = src;

    uint src_i = 0, dst_i = 0;
    while (src->len && (src->check == (src_i++ & 15))) {
        if (src->key != key) {
            memmove(dst, src, src->len);
            dst->check = dst_i++;
            dst = flash_env_next(dst);
        }
        src = flash_env_next(src);
    }
    if ((char *)(dst + 1) + len > (char *)(env + 1)) {
        errno = ENOMEM;
        return -1;
    }
    if (value) {
        dst->key = key;
        dst->len = len + sizeof(struct flash_env_item);
        dst->check = dst_i++;
        memcpy(dst->value, value, len);
        dst = flash_env_next(dst);
    }
    dst->key = 0;
    dst->len = 0;
    dst->check = dst_i++;
    return 0;
}

void flash_env_clear(struct flash_env *env) {
    memset(env, 0, sizeof(struct flash_env));
}

void flash_env_close(struct flash_env *env) {
    uint32_t flash_offset = (uintptr_t)&flash_env - XIP_BASE;
    assert((flash_offset & (FLASH_SECTOR_SIZE - 1)) == 0);
    taskENTER_CRITICAL();
    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
    flash_range_program(flash_offset, (const uint8_t *)env, FLASH_SECTOR_SIZE);
    taskEXIT_CRITICAL();

    assert(memcmp(&flash_env, env, FLASH_SECTOR_SIZE) == 0);
    free(env);
}
