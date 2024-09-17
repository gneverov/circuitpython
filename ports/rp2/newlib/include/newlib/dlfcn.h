// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#pragma once
#include <elf.h>

#include "newlib/flash_heap.h"

#define DL_FLASH_HEAP_TYPE 1

// Loader
typedef struct dl_loader_state {
    flash_heap_t heap;
    flash_ptr_t flash_base;
    flash_ptr_t ram_base;
} dl_loader_t;

int dl_loader_open(dl_loader_t *loader, uintptr_t base);
void dl_loader_free(dl_loader_t *loader);
flash_ptr_t dl_loader_relocate(const dl_loader_t *loader, flash_ptr_t addr);
int dl_loader_read(dl_loader_t *loader, void *buffer, size_t length, flash_ptr_t addr);
int dl_loader_write(dl_loader_t *loader, const void *buffer, size_t length, flash_ptr_t addr);
int dl_link(dl_loader_t *loader);


// Linker
typedef struct dl_link_state dl_linker_t;
typedef int (*dl_post_link_fun_t)(const flash_heap_header_t *header);
typedef int (*dl_link_fun_t)(const dl_linker_t *linker, dl_post_link_fun_t *post_link);

flash_ptr_t dl_linker_map(const dl_linker_t *linker, flash_ptr_t addr);
int dl_iterate_dynamic(const dl_linker_t *linker, flash_ptr_t *dyn_addr, Elf32_Dyn *dyn);
int dl_linker_read(const dl_linker_t *linker, void *buffer, size_t length, flash_ptr_t pos);
int dl_linker_write(const dl_linker_t *linker, const void *buffer, size_t length, flash_ptr_t pos);
void *dl_realloc(const dl_linker_t *linker, void *ptr, size_t size);


// Runtime API
void *dl_flash(const char *file);
bool dl_iterate(const flash_heap_header_t **header);
int dlclose(const void *handle);
char *dlerror(void);
void *dlopen(const char *file, int mode);
void *dlsym(const void *handle, const char *name);
