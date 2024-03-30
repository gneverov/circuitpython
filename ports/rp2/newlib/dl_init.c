// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <memory.h>
#include <stdint.h>

__attribute__((visibility("default")))
int __dl_main() {
    return 0;
}

__attribute__((visibility("default")))
void __dl_init(void) {
    extern const uint8_t __etext[];
    extern uint8_t __data_start__[];
    extern uint8_t __data_end__[];
    memcpy(__data_start__, __etext, __data_end__ - __data_start__);

    extern uint8_t __bss_start__[];
    extern uint8_t __bss_end__[];
    memset(__bss_start__, 0, __bss_end__ - __bss_start__);

    extern void (*__init_array_start)(void);
    extern void (*__init_array_end)(void);
    for (void(**p)(void) = &__init_array_start; p < &__init_array_end; ++p) {
        (*p)();
    }
}

__attribute__((visibility("default")))
void __dl_fini(void) {
    extern void (*__fini_array_start)(void);
    extern void (*__fini_array_end)(void);
    for (void(**p)(void) = &__fini_array_start; p < &__fini_array_end; ++p) {
        (*p)();
    }
}
