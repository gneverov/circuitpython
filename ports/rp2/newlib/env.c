// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <stdlib.h>
#include <string.h>

#include "newlib/flash_env.h"

#define ENV_MAX 64


extern char **environ;

int __real_setenv(const char *name, const char *value, int rewrite);
int __real_unsetenv(const char *name);

void env_init(void) {
    int i = 0;
    size_t len;
    const char *value;
    char *buffer = NULL;
    while ((i < ENV_MAX) && (value = flash_env_get(i, &len))) {
        i++;
        buffer = reallocf(buffer, len);
        if (!buffer) {
            continue;
        }
        memcpy(buffer, value, len);
        char *equal = strchr(buffer, '=');
        if (!equal) {
            continue;
        }
        *equal = '\0';
        __real_setenv(buffer, equal + 1, 1);
    }
    free(buffer);
}

static void env_fini(void) {
    struct flash_env *env = flash_env_open();
    if (!env) {
        return;
    }

    char **value = environ;
    int i = 0;
    while ((i < ENV_MAX) && *value) {
        flash_env_set(env, i, *value, strlen(*value) + 1);
        value++;
        i++;
    }
    while (i < ENV_MAX) {
        flash_env_set(env, i, NULL, 0);
        i++;
    }
    flash_env_close(env);
}

int __wrap_setenv(const char *name, const char *value, int rewrite) {
    int ret = __real_setenv(name, value, rewrite);
    if (ret >= 0) {
        env_fini();
    }
    return ret;
}

int __wrap_unsetenv(const char *name) {
    int ret = __real_unsetenv(name);
    if (ret >= 0) {
        env_fini();
    }
    return ret;
}
