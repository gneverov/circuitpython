#include <stdlib.h>

#include "py/gc.h"

void *malloc(size_t size) {
    return gc_alloc_possible() ? gc_alloc(size, 0, false) : NULL;
}

void free(void* ptr) {
    gc_free(ptr);
}
