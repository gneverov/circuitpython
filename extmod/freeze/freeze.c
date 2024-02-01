// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <malloc.h>
#include <memory.h>

#include "pico/platform.h"

#include "./freeze.h"
#include "py/builtin.h"
#include "py/mperrno.h"
#include "py/emitglue.h"
#include "py/gc.h"
#include "py/objint.h"
#include "py/objfun.h"
#include "py/objstr.h"
#include "py/objtype.h"


const mp_flash_page_t flash_data[NUM_FLASH_PAGES] __aligned(sizeof(mp_flash_page_t)) __in_flash("freezer");
const mp_flash_page_t ram_data_in_flash[NUM_RAM_PAGES] __aligned(sizeof(mp_flash_page_t)) __in_flash("freezer");
mp_flash_page_t ram_data[NUM_RAM_PAGES] __aligned(MICROPY_BYTES_PER_GC_BLOCK);

typedef const void *freeze_ptr_t;

STATIC size_t freeze_last_flash_size;
STATIC size_t freeze_last_ram_size;
STATIC int freeze_mode;

STATIC void freeze_writer_nlr_callback(void *ctx) {
    freeze_writer_t *self = ctx - offsetof(freeze_writer_t, nlr_callback);
    freeze_cache_flush(self, false);
}

void freeze_writer_init(freeze_writer_t *self, bool init_map) {
    self->flash_pages = flash_data;
    self->num_flash_pages = NUM_FLASH_PAGES;
    self->flash_size = freeze_last_flash_size;
    self->flash_pos = freeze_last_flash_size;
    freeze_cache_init(self);

    self->ram_pages_in_flash = ram_data_in_flash;
    self->ram_pages = ram_data;
    self->num_ram_pages = NUM_RAM_PAGES;
    self->ram_size = freeze_last_ram_size;
    self->ram_pos = self->ram_pages[0];
    self->ram_pos += freeze_last_ram_size;
    memset(self->ram_dirty, 0, sizeof(self->ram_dirty));

    self->base = self->flash_pages;

    mp_map_init(&self->obj_map, 0);

    nlr_push_jump_callback(&self->nlr_callback, freeze_writer_nlr_callback);
}

void freeze_writer_deinit(freeze_writer_t *self) {
    nlr_pop_jump_callback(true);
}

void freeze_flush(freeze_writer_t *self) {
    assert(self->flash_size <= self->num_flash_pages * sizeof(mp_flash_page_t));
    freeze_cache_flush(self, true);

    assert(self->ram_size <= self->num_ram_pages * sizeof(mp_flash_page_t));
    for (int i = 0; i < self->num_ram_pages; i++) {
        if (self->ram_dirty[i]) {
            mp_write_flash_page(&self->ram_pages_in_flash[i], &self->ram_pages[i]);
            self->ram_dirty[i] = false;
        }
    }
}

static bool freeze_in_range(freeze_ptr_t ptr, const mp_flash_page_t *base, size_t size) {
    return ((uint8_t *)ptr >= (uint8_t *)base) && ((uint8_t *)ptr <= ((uint8_t *)base) + size);
}

freeze_ptr_t freeze_tell(freeze_writer_t *self) {
    if (self->base == self->flash_pages) {
        return (uint8_t *)self->flash_pages + self->flash_pos;
    }
    else if (self->base == self->ram_pages) {
        return self->ram_pos;
    }
    else {
        assert(0);
        return NULL;
    }    
}

freeze_ptr_t freeze_seek(freeze_writer_t *self, freeze_ptr_t fptr) {
    freeze_ptr_t old_fptr = freeze_tell(self);
    if (freeze_in_range(fptr, self->flash_pages, self->flash_size)) {
        self->flash_pos = (uintptr_t)fptr - (uintptr_t)self->flash_pages;
        assert(self->flash_pos >= freeze_last_flash_size);
        self->base = self->flash_pages;
    }
    else if (freeze_in_range(fptr, self->ram_pages, self->ram_size)) {
        self->ram_pos = (void *)fptr;
        assert(self->ram_pos >= self->ram_pages[0] + freeze_last_ram_size);
        self->base = self->ram_pages;
    }
    else {
        assert(0);
    }
    return old_fptr;
}

void freeze_align(freeze_writer_t *self, size_t align) {
    assert((align & (align - 1)) == 0);
    if (self->base == self->flash_pages) {
        self->flash_pos += -self->flash_pos & (align - 1);
    }
    else if (self->base == self->ram_pages) {
        self->ram_pos += -(uintptr_t)self->ram_pos & (align - 1);
    }
    else {
        assert(0);
    }
}

void freeze_read(freeze_writer_t *self, uint8_t *buffer, size_t length) {
    if (self->base == self->flash_pages) {
        while (length > 0) {
            size_t page_num = self->flash_pos / sizeof(mp_flash_page_t);
            const mp_flash_page_t *page = self->cache_pages[page_num];
            if (!page) {
                page = &self->flash_pages[page_num];
            }
            size_t offset = self->flash_pos % sizeof(mp_flash_page_t);
            size_t len = MIN(length, sizeof(mp_flash_page_t) - offset);
            memcpy(buffer, (uint8_t *)page + offset, len);
            buffer += len;
            length -= len;
            self->flash_pos += len;
        }
    }
    else if (self->base == self->ram_pages) {
        memcpy(buffer, self->ram_pos, length);
        self->ram_pos += length;
    }
    else {
        assert(0);
    }
}

void freeze_write(freeze_writer_t *self, const uint8_t *buffer, size_t length) {
    if (self->base == self->flash_pages) {
        while (length > 0) {
            size_t page_num = self->flash_pos / sizeof(mp_flash_page_t);
            mp_flash_page_t *cache_page = freeze_cache_get(self, page_num);
            if (!cache_page) {
                mp_raise_OSError(MP_ENOMEM);
            }
            size_t offset = self->flash_pos % sizeof(mp_flash_page_t);
            size_t len = MIN(length, sizeof(mp_flash_page_t) - offset);
            memcpy((uint8_t *)cache_page + offset, buffer, len);
            buffer += len;
            length -= len;
            self->flash_pos += len;
        }
    }
    else if (self->base == self->ram_pages) {
        size_t page_num = (self->ram_pos - self->ram_pages[0]) / sizeof(mp_flash_page_t);
        memcpy(self->ram_pos, buffer, length);
        self->ram_pos += length;
        self->ram_dirty[page_num] = true;
    }
    else {
        assert(0);
    }
}

void freeze_write_char(freeze_writer_t *self, uint8_t value) {
    freeze_align(self, __alignof__(uint8_t));
    freeze_write(self, &value, sizeof(uint8_t));
}

void freeze_write_short(freeze_writer_t *self, uint16_t value) {
    freeze_align(self, __alignof__(uint16_t));
    freeze_write(self, (uint8_t *)&value, sizeof(uint16_t));
}

void freeze_write_int(freeze_writer_t *self, uint32_t value) {
    freeze_align(self, __alignof__(uint32_t));
    freeze_write(self, (uint8_t *)&value, sizeof(uint32_t));
}

void freeze_write_size(freeze_writer_t *self, size_t value) {
    freeze_align(self, __alignof__(size_t));
    freeze_write(self, (uint8_t *)&value, sizeof(size_t));
}

void freeze_write_intptr(freeze_writer_t *self, uintptr_t value) {
    freeze_align(self, __alignof__(uintptr_t));
    freeze_write(self, (uint8_t *)&value, sizeof(uintptr_t));
}

bool freeze_is_data_ptr(const void *ptr) {
    extern uint8_t __HeapLimit;
    return (ptr < (void *)&__HeapLimit);
}

void freeze_write_fptr(freeze_writer_t *self, freeze_ptr_t fptr) {
    assert(mp_is_flash_ptr(fptr) || freeze_is_data_ptr(fptr));
    freeze_write_intptr(self, (uintptr_t)fptr);
}

freeze_ptr_t freeze_allocate(freeze_writer_t *self, size_t size, size_t align, bool ram) {
    assert((align & (align - 1)) == 0);
    if (!ram) {
        self->flash_size += -self->flash_size & (align - 1);
        freeze_ptr_t fptr = (uint8_t *)self->flash_pages + self->flash_size;
        self->flash_size += size;
        if (self->flash_size > self->num_flash_pages * sizeof(mp_flash_page_t)) {
            mp_raise_msg(&mp_type_RuntimeError, "Not enough flash space");
        }
        return fptr;        
    }
    else {
        self->ram_size += -self->ram_size & (align - 1);
        freeze_ptr_t fptr = (uint8_t *)self->ram_pages + self->ram_size;
        self->ram_size += size;
        if (self->ram_size > self->num_ram_pages * sizeof(mp_flash_page_t)) {
            mp_raise_msg(&mp_type_RuntimeError, "Not enough ram space");
        }
        return fptr;        
    }
}

static void freeze_add_ptr(freeze_writer_t *self, freeze_ptr_t fptr, const void *ptr) {
    assert(ptr && !mp_is_flash_ptr(ptr) && !freeze_is_data_ptr(ptr));

    mp_map_elem_t *elem = mp_map_lookup(
        &self->obj_map,
        MP_OBJ_NEW_SMALL_INT((intptr_t)ptr),
        MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
    assert(elem->value == NULL);
    elem->value = MP_OBJ_NEW_SMALL_INT(fptr);
}

static bool freeze_lookup_ptr(freeze_writer_t *self, freeze_ptr_t *fptr, const void *ptr) {
    if (mp_is_flash_ptr(ptr) || freeze_is_data_ptr(ptr)) {
        *fptr = ptr;
        return true;
    }

    mp_map_elem_t *elem = mp_map_lookup(
        &self->obj_map,
        MP_OBJ_NEW_SMALL_INT((intptr_t)ptr),
        MP_MAP_LOOKUP);
    if (elem != NULL) {
        assert(elem->value != NULL);
        *fptr = (void *)MP_OBJ_SMALL_INT_VALUE(elem->value);
        return true;
    }

    return false;
}

typedef void (*freeze_write_t)(freeze_writer_t *self, const void *value);
typedef size_t (*freeze_sizeof_t)(const void *value);

void freeze_write_ptr(freeze_writer_t *self, const void *ptr, size_t size, size_t align, bool ram) {
    freeze_ptr_t fptr = freeze_allocate(self, size, align, ram);
    freeze_ptr_t ret = freeze_seek(self, fptr);
    freeze_write(self, ptr, size);
    freeze_seek(self, ret);

    freeze_write_fptr(self, fptr);
}

void freeze_write_aliased_ptr(freeze_writer_t *self, const void *ptr, size_t size, size_t align, bool ram) {
    freeze_ptr_t fptr;
    if (!freeze_lookup_ptr(self, &fptr, ptr)) {
        fptr = freeze_allocate(self, size, align, ram);
        freeze_add_ptr(self, fptr, ptr);
        freeze_ptr_t ret = freeze_seek(self, fptr);
        freeze_write(self, ptr, size);
        freeze_seek(self, ret);
    }
    freeze_write_fptr(self, fptr);
}

void freeze_write_obj(freeze_writer_t *self, mp_const_obj_t obj);

void freeze_write_obj_array(freeze_writer_t *self, const mp_obj_t *objs, size_t count, bool ram) {
    freeze_ptr_t fobjs = NULL;
    if (objs) {
        fobjs = freeze_allocate(self, count * sizeof(mp_obj_t), __alignof__(mp_obj_t), ram);
        freeze_ptr_t ret = freeze_seek(self, fobjs);
        for (int i = 0; i < count; i++) {
            freeze_write_obj(self, objs[i]);
        }
        freeze_seek(self, ret);
    }

    freeze_write_fptr(self, fobjs);
}

// ### base ###
void freeze_write_raw_obj(freeze_writer_t *self, const mp_obj_base_t *raw_obj);

void freeze_write_base(freeze_writer_t *self, const mp_obj_base_t *base) {
    freeze_align(self, __alignof__(mp_obj_base_t));
    freeze_write_raw_obj(self, &base->type->base);
}

// ### cell ###
extern const mp_obj_type_t mp_type_cell;

void freeze_write_cell(freeze_writer_t *self, const mp_obj_cell_t *cell) {
    assert(cell->base.type == &mp_type_cell);

    freeze_align(self, __alignof__(mp_obj_cell_t));
    freeze_write_base(self, &cell->base);
    freeze_write_obj(self, cell->obj);
}

// ### closure ###
typedef struct _mp_obj_closure_t {
    mp_obj_base_t base;
    mp_obj_t fun;
    size_t n_closed;
    mp_obj_t closed[];
} mp_obj_closure_t;

extern const mp_obj_type_t mp_type_closure;

size_t freeze_sizeof_closure(const mp_obj_closure_t *closure) {
    return closure->n_closed * sizeof(mp_obj_t);
}

void freeze_write_closure(freeze_writer_t *self, const mp_obj_closure_t *closure) {
    assert(closure->base.type == &mp_type_closure);

    freeze_align(self, __alignof__(mp_obj_closure_t));
    freeze_write_base(self, &closure->base);
    freeze_write_obj(self, closure->fun);
    freeze_write_size(self, closure->n_closed);
    for (size_t i = 0; i < closure->n_closed; i++) {
        freeze_write_obj(self, closure->closed[i]);
    }
}

// ### dict ###
void freeze_write_map(freeze_writer_t *self, const mp_map_t *map, bool mutable) {
    union mp_map_header {
        mp_map_t map;
        size_t header;
    } fmap = { .map = *map };
    fmap.map.is_fixed |= !mutable;

    freeze_align(self, __alignof__(mp_map_t));
    freeze_write_size(self, fmap.header);
    freeze_write_size(self, map->alloc);
    freeze_write_obj_array(self, (mp_obj_t *)map->table, 2 * map->alloc, mutable);
}

void freeze_write_immutable_dict_ptr(freeze_writer_t *self, const mp_obj_dict_t *dict) {
    assert(dict->base.type == &mp_type_dict);

    freeze_ptr_t fdict = freeze_allocate(self, sizeof(mp_obj_dict_t), __alignof__(mp_obj_dict_t), false);
    freeze_ptr_t ret = freeze_seek(self, fdict);
    freeze_write_base(self, &dict->base);
    freeze_write_map(self, &dict->map, false);
    freeze_seek(self, ret);
    freeze_write_fptr(self, fdict);    
}

static void freeze_write_mutable_dict(freeze_writer_t *self, const mp_obj_dict_t *dict) {
    assert(dict->base.type == &mp_type_dict);

    freeze_write_base(self, &dict->base);
    freeze_write_map(self, &dict->map, true);
}

// ### fun_bc ###
freeze_ptr_t freeze_new_raw_code(freeze_writer_t *self, const mp_raw_code_t *rc) {
    freeze_ptr_t frc;
    if (!freeze_lookup_ptr(self, &frc, rc)) {
        frc = freeze_allocate(self, sizeof(mp_raw_code_t), __alignof__(mp_raw_code_t), false);
        freeze_ptr_t ret = freeze_seek(self, frc);
        freeze_add_ptr(self, frc, rc);

        freeze_write_int(self, *(mp_uint_t *)rc);

        freeze_write_ptr(self, rc->fun_data, rc->fun_data_len, __alignof__(char), false);
        
        freeze_write_size(self, rc->fun_data_len);

        freeze_ptr_t fchild_table = NULL;
        if (rc->children) {
            fchild_table = freeze_allocate(self, rc->n_children * sizeof(mp_raw_code_t *), __alignof__(mp_raw_code_t *), false);
            freeze_ptr_t ret = freeze_seek(self, fchild_table);
            for (int i = 0; i < rc->n_children; i++) {
                freeze_ptr_t fchild_rc = freeze_new_raw_code(self, rc->children[i]);
                freeze_write_fptr(self, fchild_rc);
            }
            freeze_seek(self, ret);
        }
        freeze_write_fptr(self, fchild_table);

        freeze_write_size(self, rc->n_children);

#if MICROPY_EMIT_MACHINE_CODE
        freeze_write_short(self, rc->prelude_offset);

        freeze_write_int(self, rc->type_sig);
#endif

        freeze_seek(self, ret);
    }
    return frc;
}

size_t freeze_sizeof_fun_bc(const mp_obj_fun_bc_t *fun_bc) {
    return fun_bc->n_extra_args * sizeof(mp_obj_t);
}

void freeze_write_fun_bc(freeze_writer_t *self, const mp_obj_fun_bc_t *fun_bc) {
    assert((fun_bc->base.type == &mp_type_fun_bc) || (fun_bc->base.type == &mp_type_gen_wrap));

    freeze_align(self, __alignof__(mp_obj_fun_bc_t));
    freeze_write_base(self, &fun_bc->base);
    
    freeze_write_raw_obj(self, &fun_bc->context->module.base);

    freeze_ptr_t frc = freeze_new_raw_code(self, fun_bc->rc);
    mp_raw_code_t rc;
    if (freeze_in_range(frc, self->flash_pages, self->flash_size)) {
        freeze_ptr_t ret = freeze_seek(self, frc);
        freeze_read(self, (uint8_t *)&rc, sizeof(mp_raw_code_t));
        freeze_seek(self, ret);
    }
    else {
        rc = *(mp_raw_code_t *)frc;
    }
    freeze_write_fptr(self, rc.children);
    freeze_write_fptr(self, rc.fun_data);
    freeze_write_fptr(self, frc);

    freeze_write_size(self, fun_bc->n_extra_args);

    for (size_t i = 0; i < fun_bc->n_extra_args; i++) {
        if (mp_obj_is_dict_or_ordereddict(fun_bc->extra_args[i])) {
            const mp_obj_dict_t *dict = MP_OBJ_TO_PTR(fun_bc->extra_args[i]);
            freeze_write_immutable_dict_ptr(self, dict);
        } else {
            freeze_write_obj(self, fun_bc->extra_args[i]);
        }
    }
}

// ### bound_meth ###
typedef struct _mp_obj_bound_meth_t {
    mp_obj_base_t base;
    mp_obj_t meth;
    mp_obj_t self;
} mp_obj_bound_meth_t;

extern const mp_obj_type_t mp_type_bound_meth;

void freeze_write_bound_meth(freeze_writer_t *self, const mp_obj_bound_meth_t *bound_meth) {
    assert((bound_meth->base.type == &mp_type_bound_meth));

    freeze_align(self, __alignof__(mp_obj_bound_meth_t));
    freeze_write_base(self, &bound_meth->base);
    freeze_write_obj(self, bound_meth->meth);
    freeze_write_obj(self, bound_meth->self);
}

// ### float ###
typedef struct _mp_obj_float_t {
    mp_obj_base_t base;
    mp_float_t value;
} mp_obj_float_t;

void freeze_write_float_obj(freeze_writer_t *self, const mp_obj_float_t *float_obj) {
    assert(float_obj->base.type == &mp_type_float);
    freeze_align(self, __alignof__(mp_obj_float_t));
    freeze_write_base(self, &float_obj->base);
    freeze_align(self, __alignof__(mp_float_t));
    freeze_write(self, (uint8_t *)&float_obj->value, sizeof(mp_float_t));
}

// ### int ###
void freeze_write_mpz(freeze_writer_t *self, const mpz_t *mpz) {
    freeze_align(self, __alignof__(mpz_t));
    freeze_write_size(self, *(size_t *)mpz);
    freeze_write_size(self, mpz->len);
    freeze_write_ptr(self, mpz->dig, mpz->len * sizeof(mpz_dig_t), __alignof__(mpz_dig_t), false);
}

void freeze_write_int_obj(freeze_writer_t *self, const mp_obj_int_t *int_obj) {
    assert(int_obj->base.type == &mp_type_int);
    freeze_align(self, __alignof__(mp_obj_int_t));
    freeze_write_base(self, &int_obj->base);
    freeze_write_mpz(self, &int_obj->mpz);
}

// ### module ###
void freeze_write_module(freeze_writer_t *self, const mp_obj_module_t *module) {
    assert(module->base.type == &mp_type_module);

    mp_obj_t dest[2];
    mp_load_method_maybe(MP_OBJ_FROM_PTR(module), MP_QSTR___path__, dest);
    bool is_package = dest[0] != MP_OBJ_NULL;

    freeze_align(self, __alignof__(mp_obj_module_t));
    freeze_write_base(self, &module->base);
    if (is_package) {
        freeze_write_raw_obj(self, &module->globals->base);
    }
    else {
        freeze_write_immutable_dict_ptr(self, module->globals);
    }
}

void freeze_write_module_context(freeze_writer_t *self, const mp_module_context_t *context) {
    // printf("module_context: n_qstr=%d, n_obj=%d\n", context->constants.n_qstr, context->constants.n_obj);

    freeze_align(self, __alignof__(mp_module_context_t));
    freeze_write_module(self, &context->module);

    freeze_align(self, __alignof__(mp_module_constants_t));
    freeze_write_ptr(self, context->constants.qstr_table, context->constants.n_qstr * sizeof(qstr_short_t), __alignof__(qstr_short_t), false);
    freeze_write_obj_array(self, context->constants.obj_table, context->constants.n_obj, false);
    freeze_write_size(self, context->constants.n_qstr);
    freeze_write_size(self, context->constants.n_obj);
}

freeze_ptr_t freeze_new_module(freeze_writer_t *self, mp_obj_t module_obj) {
    const mp_module_context_t *module = MP_OBJ_TO_PTR(module_obj);
    freeze_ptr_t fmodule;
    if (freeze_lookup_ptr(self, &fmodule, module)) {
        return fmodule;
    }

    fmodule = freeze_allocate(self, sizeof(mp_module_context_t), __alignof(mp_module_context_t), false);
    freeze_add_ptr(self, fmodule, module);
    freeze_ptr_t ret = freeze_seek(self, fmodule);
    freeze_write_module_context(self, module);
    freeze_seek(self, ret);
    return fmodule;
}

void freeze_write_non_frozen_module(freeze_writer_t *self, const mp_module_context_t *context) {
    mp_obj_t dest[2];
    mp_load_method_maybe(MP_OBJ_FROM_PTR(context), MP_QSTR___name__, dest);
    mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("module '%s' already loaded but not frozen"), mp_obj_str_get_str(dest[0]));
}

// ### property ###
typedef struct _mp_obj_property_t {
    mp_obj_base_t base;
    mp_obj_t proxy[3]; // getter, setter, deleter
} mp_obj_property_t;

void freeze_write_property(freeze_writer_t *self, const mp_obj_property_t *property) {
    assert(property->base.type == &mp_type_property);

    freeze_align(self, __alignof__(mp_obj_property_t));
    freeze_write_base(self, &property->base);
    freeze_write_obj(self, property->proxy[0]);
    freeze_write_obj(self, property->proxy[1]);
    freeze_write_obj(self, property->proxy[2]);
}

// ### static/class method ###
void freeze_write_static_class_method(freeze_writer_t *self, const mp_obj_static_class_method_t *scm) {
    assert((scm->base.type == &mp_type_staticmethod) || (scm->base.type == &mp_type_classmethod));

    freeze_align(self, __alignof__(mp_obj_static_class_method_t));
    freeze_write_base(self, &scm->base);
    freeze_write_obj(self, scm->fun);
}

// ### str ###
void freeze_write_str(freeze_writer_t *self, const mp_obj_str_t *str) {
    assert((str->base.type == &mp_type_str) || (str->base.type == &mp_type_bytes));

    freeze_align(self, __alignof__(mp_obj_str_t));
    freeze_write_base(self, &str->base);
    freeze_write_size(self, str->hash);
    freeze_write_size(self, str->len);
    freeze_write_aliased_ptr(self, str->data, str->len + 1, __alignof__(char), false);
}

// ### tuple ###
size_t freeze_sizeof_tuple(const mp_obj_tuple_t *tuple) {
    return tuple->len * sizeof(mp_obj_t);
}

void freeze_write_tuple(freeze_writer_t *self, const mp_obj_tuple_t *tuple) {
    assert(tuple->base.type == &mp_type_tuple);

    freeze_align(self, __alignof__(mp_obj_tuple_t));
    freeze_write_base(self, &tuple->base);
    freeze_write_size(self, tuple->len);
    for (size_t i = 0; i < tuple->len; i++) {
        freeze_write_obj(self, tuple->items[i]);
    }
}

// ### type ###
static size_t freeze_type_num_slots(const mp_obj_type_t *type) {
    size_t n = 0;
    n = MAX(n, type->slot_index_make_new);
    n = MAX(n, type->slot_index_print);
    n = MAX(n, type->slot_index_call);
    n = MAX(n, type->slot_index_unary_op);
    n = MAX(n, type->slot_index_binary_op);
    n = MAX(n, type->slot_index_attr);
    n = MAX(n, type->slot_index_subscr);
    n = MAX(n, type->slot_index_iter);
    n = MAX(n, type->slot_index_buffer);
    n = MAX(n, type->slot_index_protocol);
    n = MAX(n, type->slot_index_parent);
    n = MAX(n, type->slot_index_locals_dict);
    return n;
}

size_t freeze_sizeof_type(const mp_obj_type_t *type) {
    return freeze_type_num_slots(type) * sizeof(void *);
}

void freeze_write_type(freeze_writer_t *self, const mp_obj_type_t *type) {
    assert(type->base.type == &mp_type_type);

    freeze_align(self, __alignof__(mp_obj_type_t));
    freeze_write_base(self, &type->base);
    freeze_write_short(self, type->flags);
    freeze_write_short(self, type->name);
    freeze_write_char(self, type->slot_index_make_new);
    freeze_write_char(self, type->slot_index_print);
    freeze_write_char(self, type->slot_index_call);
    freeze_write_char(self, type->slot_index_unary_op);
    freeze_write_char(self, type->slot_index_binary_op);
    freeze_write_char(self, type->slot_index_attr);
    freeze_write_char(self, type->slot_index_subscr);
    freeze_write_char(self, type->slot_index_iter);
    freeze_write_char(self, type->slot_index_buffer);
    freeze_write_char(self, type->slot_index_protocol);
    freeze_write_char(self, type->slot_index_parent);
    freeze_write_char(self, type->slot_index_locals_dict);

    size_t n_slots = freeze_type_num_slots(type);
    for (size_t i = 0; i < n_slots; i++) {
        size_t slot = i + 1;
        if (slot == type->slot_index_locals_dict) {
            const mp_obj_dict_t *locals_dict = type->slots[i];
            freeze_write_immutable_dict_ptr(self, locals_dict);
        } else if (slot == type->slot_index_parent) {
            const mp_obj_base_t *parent = type->slots[i];
            freeze_write_raw_obj(self, parent);
        } else {
            freeze_write_fptr(self, type->slots[i]);
        }
    }
}

// ### instance ###
size_t freeze_sizeof_instance(const mp_obj_instance_t *obj) {
    const mp_obj_type_t *native_base;
    size_t num_native_bases = instance_count_native_bases(obj->base.type, &native_base);
    return num_native_bases * sizeof(native_base);
}

void freeze_write_instance(freeze_writer_t *self, const mp_obj_instance_t *obj) {
    assert(obj->base.type->flags & MP_TYPE_FLAG_INSTANCE_TYPE);

    const mp_obj_type_t *native_base;
    size_t num_native_bases = instance_count_native_bases(obj->base.type, &native_base);
    freeze_align(self, __alignof__(mp_obj_instance_t));
    freeze_write_base(self, &obj->base);
    freeze_write_map(self, &obj->members, true);

    for (size_t i = 0; i < num_native_bases; i++) {
        freeze_write_obj(self, obj->subobj[i]);
    }
}

// ## list ##
void freeze_write_list(freeze_writer_t *self, const mp_obj_list_t *list) {
    assert(list->base.type == &mp_type_list);

    freeze_align(self, __alignof__(mp_obj_list_t));
    freeze_write_base(self, &list->base);
    freeze_write_size(self, list->alloc);
    freeze_write_size(self, list->len);
    freeze_write_obj_array(self, list->items, list->alloc, true);
}

// ## set ##
typedef struct _mp_obj_set_t {
    mp_obj_base_t base;
    mp_set_t set;
} mp_obj_set_t;

void freeze_write_set(freeze_writer_t *self, const mp_obj_set_t *set) {
    assert(set->base.type == &mp_type_set);

    freeze_align(self, __alignof__(mp_obj_set_t));
    freeze_write_base(self, &set->base);
    freeze_write_size(self, set->set.alloc);
    freeze_write_size(self, set->set.used);
    freeze_write_obj_array(self, set->set.table, set->set.alloc, true);
}

#if MICROPY_PY_RE
#include "lib/re1.5/re1.5.h"

// ### re ###
typedef struct _mp_obj_re_t {
    mp_obj_base_t base;
    ByteProg re;
} mp_obj_re_t;

extern const mp_obj_type_t re_type;

size_t freeze_sizeof_re(const mp_obj_re_t *re) {
    return re->re.bytelen;
}

void freeze_write_re(freeze_writer_t *self, const mp_obj_re_t *re) {
    assert(re->base.type == &re_type);

    freeze_align(self, __alignof__(mp_obj_re_t));
    freeze_write_base(self, &re->base);
    freeze_write_int(self, re->re.bytelen);
    freeze_write_int(self, re->re.len);
    freeze_write_int(self, re->re.sub);
    freeze_write(self, (uint8_t *)re->re.insts, re->re.bytelen);
}
#endif

// ### raw_obj ###
struct freeze_type {
    const mp_obj_type_t *type;
    size_t size;
    size_t align;
    bool mutable;
    freeze_write_t writer;
    freeze_sizeof_t var_sizeof;
} const freeze_type_table[] = {
    { &mp_type_fun_bc, sizeof(mp_obj_fun_bc_t), __alignof__(mp_obj_fun_bc_t), false, (freeze_write_t)freeze_write_fun_bc, (freeze_sizeof_t)freeze_sizeof_fun_bc },
    { &mp_type_tuple, sizeof(mp_obj_tuple_t), __alignof__(mp_obj_tuple_t), false, (freeze_write_t)freeze_write_tuple, (freeze_sizeof_t)freeze_sizeof_tuple },
    { &mp_type_property, sizeof(mp_obj_property_t), __alignof__(mp_obj_property_t), false, (freeze_write_t)freeze_write_property, NULL },
    { &mp_type_type, sizeof(mp_obj_type_t), __alignof__(mp_obj_type_t), false, (freeze_write_t)freeze_write_type, (freeze_sizeof_t)freeze_sizeof_type },
    { &mp_type_module, sizeof(mp_module_context_t), __alignof__(mp_module_context_t), false, (freeze_write_t)freeze_write_non_frozen_module, NULL },

    { &mp_type_gen_wrap, sizeof(mp_obj_fun_bc_t), __alignof__(mp_obj_fun_bc_t), false, (freeze_write_t)freeze_write_fun_bc, (freeze_sizeof_t)freeze_sizeof_fun_bc },
    { &mp_type_bound_meth, sizeof(mp_obj_bound_meth_t), __alignof__(mp_obj_bound_meth_t), false, (freeze_write_t)freeze_write_bound_meth, NULL },
    { &mp_type_staticmethod, sizeof(mp_obj_static_class_method_t), __alignof__(mp_obj_static_class_method_t), false, (freeze_write_t)freeze_write_static_class_method, NULL },
    { &mp_type_classmethod, sizeof(mp_obj_static_class_method_t), __alignof__(mp_obj_static_class_method_t), false, (freeze_write_t)freeze_write_static_class_method, NULL },

    { &mp_type_str, sizeof(mp_obj_str_t), __alignof__(mp_obj_str_t), false, (freeze_write_t)freeze_write_str, NULL },
    { &mp_type_bytes, sizeof(mp_obj_str_t), __alignof__(mp_obj_str_t), false, (freeze_write_t)freeze_write_str, NULL },

    { &mp_type_closure, sizeof(mp_obj_closure_t), __alignof__(mp_obj_closure_t), false, (freeze_write_t)freeze_write_closure, (freeze_sizeof_t)freeze_sizeof_closure },
    { &mp_type_cell, sizeof(mp_obj_cell_t), __alignof__(mp_obj_cell_t), true, (freeze_write_t)freeze_write_cell, NULL },
    { &mp_type_int, sizeof(mp_obj_int_t), __alignof__(mp_obj_int_t), false, (freeze_write_t)freeze_write_int_obj, NULL },
    { &mp_type_float, sizeof(mp_obj_float_t), __alignof__(mp_obj_float_t), false, (freeze_write_t)freeze_write_float_obj, NULL },
    { &mp_type_object, sizeof(mp_obj_base_t), __alignof__(mp_obj_base_t), false, (freeze_write_t)freeze_write_base, NULL },

    { &mp_type_dict, sizeof(mp_obj_dict_t), __alignof__(mp_obj_dict_t), true, (freeze_write_t)freeze_write_mutable_dict, NULL },
    { &mp_type_list, sizeof(mp_obj_list_t), __alignof__(mp_obj_list_t), true, (freeze_write_t)freeze_write_list, NULL },
    { &mp_type_set, sizeof(mp_obj_set_t), __alignof__(mp_obj_set_t), true, (freeze_write_t)freeze_write_set, NULL },

    { &re_type, sizeof(mp_obj_re_t), __alignof__(mp_obj_re_t), false, (freeze_write_t)freeze_write_re, (freeze_sizeof_t)freeze_sizeof_re },
    
    { NULL }    
},
freeze_type_instance = { NULL, sizeof(mp_obj_instance_t), __alignof__(mp_obj_instance_t), true, (freeze_write_t)freeze_write_instance, (freeze_sizeof_t)freeze_sizeof_instance };

const struct freeze_type *freeze_get_type(const mp_obj_type_t *type) { 
    const struct freeze_type *ftype = freeze_type_table;
    while (ftype->type) {
        if (type == ftype->type) {
            return ftype;
        }
        ftype++;
    }
    if (type->flags & MP_TYPE_FLAG_INSTANCE_TYPE) {
        return &freeze_type_instance;
    }
    return NULL;
}

freeze_ptr_t freeze_new_raw_obj(freeze_writer_t *self, const mp_obj_base_t *raw_obj) {
    if (raw_obj == NULL) {
        return NULL;
    }

    freeze_ptr_t fraw_obj;
    if (freeze_lookup_ptr(self, &fraw_obj, raw_obj)) {
        return fraw_obj;
    }

    const struct freeze_type *ftype = freeze_get_type(raw_obj->type);
    if (!ftype) {
        mp_raise_msg_varg(&mp_type_TypeError, MP_ERROR_TEXT("don't know how to freeze '%q'"), raw_obj->type->name);
    }

    size_t size = ftype->size;
    if (ftype->var_sizeof) {
        size += ftype->var_sizeof(raw_obj);
    }
    fraw_obj = freeze_allocate(self, size, ftype->align, ftype->mutable);
    freeze_add_ptr(self, fraw_obj, raw_obj);
    freeze_ptr_t ret = freeze_seek(self, fraw_obj);
    ftype->writer(self, raw_obj);
    freeze_seek(self, ret);
    return fraw_obj;
}

void freeze_write_raw_obj(freeze_writer_t *self, const mp_obj_base_t *raw_obj) {
    freeze_ptr_t fraw_obj = freeze_new_raw_obj(self, raw_obj);
    freeze_write_fptr(self, fraw_obj);
}

void freeze_write_obj(freeze_writer_t *self, mp_const_obj_t obj) {
    if (obj == MP_OBJ_NULL) {
        freeze_write_intptr(self, (uintptr_t)obj);
    } else if (mp_obj_is_small_int(obj)) {
        freeze_write_intptr(self, (uintptr_t)obj);
    } else if (mp_obj_is_qstr(obj)) {
        freeze_write_intptr(self, (uintptr_t)obj);
    } else if (mp_obj_is_immediate_obj(obj)) {
        freeze_write_intptr(self, (uintptr_t)obj);
    } else if (mp_obj_is_obj(obj)) {
        const mp_obj_base_t *raw_obj = MP_OBJ_TO_PTR(obj);
        freeze_ptr_t fraw_obj = freeze_new_raw_obj(self, raw_obj);
        freeze_write_intptr(self, (uintptr_t)MP_OBJ_FROM_PTR(fraw_obj));
    } else {
        assert(0);
    }
}

// ### qstr ###
enum qstr_pool_field {
    QSTR_POOL_FIELD_HASHES,
    QSTR_POOL_FIELD_LENGTHS,
    QSTR_POOL_FIELD_QSTRS,
};

void freeze_write_qstr_pool(freeze_writer_t *self, const qstr_pool_t *pool, enum qstr_pool_field field) {
    if (mp_is_flash_ptr(pool)) {
        return;
    }
    if (pool->prev != NULL) {
        freeze_write_qstr_pool(self, pool->prev, field);
    }
    switch (field)
    {
        case QSTR_POOL_FIELD_HASHES:
            freeze_write(self, pool->hashes, pool->len * sizeof(qstr_hash_t));
            break;

        case QSTR_POOL_FIELD_LENGTHS:
            freeze_write(self, pool->lengths, pool->len * sizeof(qstr_len_t));
            break;

        case QSTR_POOL_FIELD_QSTRS:
            for (size_t i = 0; i < pool->len; i++) {
                freeze_write_ptr(self, pool->qstrs[i], pool->lengths[i] + 1, __alignof__(char), false);
            }
            break;

        default:
            assert(0);
    }
}

freeze_ptr_t freeze_new_qstr_pool(freeze_writer_t *self, const qstr_pool_t *last_pool) {
    const qstr_pool_t *first_pool = last_pool;
    while (!mp_is_flash_ptr(first_pool) && first_pool->prev != NULL) {
        first_pool = first_pool->prev;
    }

    size_t total_prev_len = first_pool->total_prev_len + first_pool->len;
    size_t len = last_pool->total_prev_len + last_pool->len - total_prev_len;
    if (len == 0) {
        return NULL;
    }

    freeze_ptr_t fpool = freeze_allocate(self, sizeof(qstr_pool_t) + len * sizeof(uint8_t *), __alignof__(qstr_pool_t), false);
    freeze_ptr_t fhashes = freeze_allocate(self, len * sizeof(qstr_hash_t), __alignof__(qstr_hash_t), false);
    freeze_ptr_t flengths = freeze_allocate(self, len * sizeof(qstr_len_t), __alignof__(qstr_len_t), false);

    freeze_ptr_t ret = freeze_seek(self, fpool);
    freeze_write_fptr(self, first_pool);
    freeze_write_size(self, total_prev_len);
    freeze_write_size(self, 10);
    freeze_write_size(self, len);
    freeze_write_fptr(self, fhashes);
    freeze_write_fptr(self, flengths);
    freeze_write_qstr_pool(self, last_pool, QSTR_POOL_FIELD_QSTRS);
    
    freeze_seek(self, fhashes);
    freeze_write_qstr_pool(self, last_pool, QSTR_POOL_FIELD_HASHES);

    freeze_seek(self, flengths);
    freeze_write_qstr_pool(self, last_pool, QSTR_POOL_FIELD_LENGTHS);

    freeze_seek(self, ret);
    return fpool;
}

// ### api ###
enum freeze_header_type {
    FREEZE_MODULE = 1,
    FREEZE_QSTR_POOL = 2,
};

typedef struct {
    uint16_t flash_size;
    uint16_t ram_size;
    uint16_t type;
    const void* object;
} freeze_header_t;

STATIC void freeze_write_sentinel(freeze_writer_t *self) {
    freeze_ptr_t sentinel = freeze_allocate(self, sizeof(freeze_header_t), __alignof(freeze_header_t), false);
    freeze_ptr_t ret = freeze_seek(self, sentinel);
    freeze_write_short(self, 0);
    freeze_write_short(self, 0);
    freeze_write_short(self, 0);
    freeze_write_fptr(self, NULL);
    freeze_seek(self, ret);
    self->flash_size -= sizeof(freeze_header_t);
}

bool freeze_clear() {
    if (freeze_mode > 0) {
        return false;
    }
    freeze_mode = -1;
    freeze_last_flash_size = 0;
    freeze_last_ram_size = 0;

    freeze_writer_t freezer;
    freeze_writer_init(&freezer, false);
    freeze_write_sentinel(&freezer);
    freeze_flush(&freezer);
    freeze_writer_deinit(&freezer);
    return true;
}

void freeze_gc() {
    gc_collect_root((void**)ram_data, freeze_last_ram_size / sizeof(void *));
}

STATIC bool freeze_header_next(const freeze_header_t **pheader) {
    const freeze_header_t *header = *pheader;
    if (header == NULL) {
        *pheader = (freeze_header_t *)flash_data[0];
    }
    else {
        *pheader = (freeze_header_t *)(((uint8_t *)header) + header->flash_size);
    }
    return (*pheader)->flash_size;
}

void freeze_init() {
    static_assert(NUM_RAM_PAGES == 1);
    mp_read_flash_page(ram_data, ram_data_in_flash);

    freeze_mode = 0;
    freeze_last_flash_size = 0;
    freeze_last_ram_size = 0;

    const freeze_header_t *header = NULL;
    while (freeze_header_next(&header)) {
        if (header->type == FREEZE_QSTR_POOL) {
            qstr_pool_t *qstr_pool = (qstr_pool_t *)header->object;
            assert(qstr_pool->prev == MP_STATE_VM(last_pool));
            MP_STATE_VM(last_pool) = qstr_pool;
            MP_STATE_VM(qstr_last_chunk) = NULL;
            MP_STATE_VM(qstr_last_alloc) = 0;
            MP_STATE_VM(qstr_last_used) = 0;
        }

        freeze_last_flash_size += header->flash_size;
        freeze_last_ram_size += header->ram_size;
    }
}

STATIC void freeze_write_header(freeze_writer_t *self, enum freeze_header_type header_type, freeze_ptr_t header_object) {
    freeze_allocate(self, 0, __alignof(freeze_header_t), false);

    size_t flash_size = self->flash_size - freeze_last_flash_size;
    assert(flash_size <= 0xffff);
    freeze_write_short(self, flash_size);
    size_t ram_size = self->ram_size - freeze_last_ram_size;
    assert(ram_size <= 0xffff);
    freeze_write_short(self, ram_size);
    freeze_write_short(self, header_type);
    freeze_write_fptr(self, header_object);

    freeze_write_sentinel(self);

    freeze_flush(self);

    freeze_last_flash_size = self->flash_size;
    freeze_last_ram_size = self->ram_size;
}

mp_obj_t mp_module_get_frozen(qstr module_name, mp_obj_t outer_module_obj) {
    const freeze_header_t *header = NULL;
    while (freeze_header_next(&header)) {
        if (header->type == FREEZE_MODULE) {
            mp_obj_t module_obj = MP_OBJ_FROM_PTR(header->object);
            mp_obj_t module_name_obj = mp_load_attr(module_obj, MP_QSTR___name__);
            if (MP_OBJ_QSTR_VALUE(module_name_obj) == module_name) {
                mp_map_t *module_map = &MP_STATE_VM(mp_loaded_modules_dict).map;
                mp_map_elem_t *elem = mp_map_lookup(module_map, module_name_obj, MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
                elem->value = module_obj;
                return module_obj;
            }
        }
    }
    return MP_OBJ_NULL;
}

mp_obj_t mp_module_freeze(qstr module_name, mp_obj_t module_obj, mp_obj_t outer_module_obj) {
    if (freeze_mode < 1) {
        return module_obj;
    }

    freeze_writer_t freezer;
    freeze_writer_init(&freezer, true);
    freeze_ptr_t fheader = freeze_allocate(&freezer, sizeof(freeze_header_t), __alignof(freeze_header_t), false);
    freeze_ptr_t fmodule = freeze_new_module(&freezer, module_obj);
        
    freeze_seek(&freezer, fheader);
    freeze_write_header(&freezer, FREEZE_MODULE, fmodule);
    freeze_writer_deinit(&freezer);
    
    module_obj = (void *)fmodule;
    mp_map_t *mp_loaded_modules_map = &MP_STATE_VM(mp_loaded_modules_dict).map;
    mp_map_elem_t *elem = mp_map_lookup(mp_loaded_modules_map, MP_OBJ_NEW_QSTR(module_name), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
    elem->value = module_obj;
    return module_obj;
}

STATIC void freeze_mode_nlr_callback(void *ctx) {
    freeze_mode--;
}

mp_obj_t freeze_import(size_t n_args, const mp_obj_t *args) {
    if (freeze_mode < 0) {
        return MP_OBJ_NULL;
    }

    mp_obj_t result = mp_obj_new_tuple(n_args, NULL);
    size_t len;
    mp_obj_t *items;
    mp_obj_tuple_get(result, &len, &items);

    freeze_mode++;
    size_t entry_flash_size = freeze_last_flash_size;
    size_t entry_ram_size = freeze_last_ram_size;
    nlr_jump_callback_node_t nlr_callback;
    nlr_push_jump_callback(&nlr_callback, freeze_mode_nlr_callback);
    for (size_t i = 0; i <n_args; i++) {
        items[i] = mp_builtin___import__(1, &args[i]);
    }
    nlr_pop_jump_callback(true);

    freeze_writer_t freezer;
    freeze_writer_init(&freezer, false);
    freeze_ptr_t fheader = freeze_allocate(&freezer, sizeof(freeze_header_t), __alignof(freeze_header_t), false);
    freeze_ptr_t fpool = freeze_new_qstr_pool(&freezer, MP_STATE_VM(last_pool));
    if (fpool) {
        freeze_seek(&freezer, fheader);
        freeze_write_header(&freezer, FREEZE_QSTR_POOL, fpool);
    }
    freeze_writer_deinit(&freezer);

    mp_printf(&mp_plat_print, "froze %u flash bytes, %u ram bytes\n", freeze_last_flash_size - entry_flash_size, freeze_last_ram_size - entry_ram_size);
    return result;
}

mp_obj_t freeze_modules(void) {
    mp_obj_t dict = mp_obj_new_dict(0);
    const freeze_header_t *header = NULL;
    while (freeze_header_next(&header)) {
        if (header->type == FREEZE_MODULE) {
            mp_obj_t module_obj = MP_OBJ_FROM_PTR(header->object);
            mp_obj_t module_name_obj = mp_load_attr(module_obj, MP_QSTR___name__);
            mp_obj_dict_store(dict, module_name_obj, module_obj);
        }
    }
    return dict;
}