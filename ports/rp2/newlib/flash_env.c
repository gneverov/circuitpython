// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define FLASH_ENV_MAX 64
#define FLASH_ENV_OFFSET 0x1000


struct flash_env {
    const char *env[FLASH_ENV_MAX];
    uintptr_t nenv[FLASH_ENV_MAX];
    char buffer[3584];
};

static_assert(sizeof(struct flash_env) == 4096);

extern char **environ;

__attribute__((section(".flash_env")))
static const volatile struct flash_env flash_env;

__attribute__((constructor(101), visibility("hidden")))
void env_init(void) {
    for (int i = 0; i < FLASH_ENV_MAX; i++) {
        if ((uintptr_t)flash_env.env[i] != ~flash_env.nenv[i]) {
            return;
        }
    }
    environ = (char **)flash_env.env;
}

static int env_compare(void) {
    if (environ == (char **)flash_env.env) {
        return 0;
    }
    char **e = environ;
    for (int i = 0; i < FLASH_ENV_MAX; i++) {
        if (*e != flash_env.env[i]) {
            return 1;
        }
        if (*e) {
            e++;
        }
    }
    return 0;
}

__attribute__((destructor, visibility("hidden")))
int env_fini(void) {
    if (!env_compare()) {
        return 0;
    }

    int fd = open("/dev/firmware", O_RDWR);
    if (fd < 0) {
        return -1;
    }
    int ret = -1;
    void *addr = mmap(0, sizeof(struct flash_env), PROT_READ, MAP_SHARED, fd, FLASH_ENV_OFFSET);
    if (addr != &flash_env) {
        goto exit;
    }

    off_t pos = offsetof(struct flash_env, buffer);
    char **e = environ;
    for (int i = 0; i < FLASH_ENV_MAX; i++) {
        uintptr_t ptr = *e ? pos + (uintptr_t)&flash_env : 0;
        if (pwrite(fd, &ptr, sizeof(ptr), FLASH_ENV_OFFSET + offsetof(struct flash_env, env[i])) < 0) {
            goto exit;
        }
        ptr = ~ptr;
        if (pwrite(fd, &ptr, sizeof(ptr), FLASH_ENV_OFFSET + offsetof(struct flash_env, nenv[i])) < 0) {
            goto exit;
        }
        if (*e) {
            size_t len = strlen(*e) + 1;
            if (pwrite(fd, *e, len, FLASH_ENV_OFFSET + pos) < 0) {
                goto exit;
            }
            pos += len;
            e++;
        }
    }
    if (fsync(fd) < 0) {
        goto exit;
    }
    ret = 0;
exit:
    close(fd);
    return ret;
}
