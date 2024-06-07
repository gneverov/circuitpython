// SPDX-FileCopyrightText: 2023 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <elf.h>
#include <malloc.h>
#include <memory.h>

#include "newlib/dlfcn.h"
#include "newlib/flash_heap.h"
#include "pico/platform.h"

#include "./freeze.h"
#include "./extmod.h"
#include "py/builtin.h"
#include "py/mperrno.h"
#include "py/emitglue.h"
#include "py/gc.h"
#include "py/objint.h"
#include "py/objfun.h"
#include "py/objstr.h"
#include "py/objtype.h"
#include "py/smallint.h"


static uint8_t ram_data[2048] __aligned(MICROPY_BYTES_PER_GC_BLOCK);
static size_t freeze_last_ram_size;
static int freeze_mode;
static const flash_heap_header_t *freeze_checkpoint;

static void freeze_writer_nlr_callback(void *ctx) {
    freeze_writer_t *self = ctx - offsetof(freeze_writer_t, nlr_callback);
    flash_heap_free(&self->heap);
}

static void freeze_writer_init(freeze_writer_t *self, uint32_t type) {
    if (flash_heap_open(&self->heap, type) < 0) {
        mp_raise_OSError(errno);
    }
    self->ram_start = ram_data + freeze_last_ram_size;
    self->ram_end = self->ram_start;
    self->ram_pos = 0;
    self->ram_limit = ram_data + sizeof(ram_data);

    mp_map_init(&self->obj_map, 0);
    nlr_push_jump_callback(&self->nlr_callback, freeze_writer_nlr_callback);
    if (flash_heap_get_header(&self->heap) < freeze_checkpoint) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("reset needed"));
    }    
}

static void freeze_writer_deinit(freeze_writer_t *self) {
    nlr_pop_jump_callback(true);
}

static void freeze_writer_commit(freeze_writer_t *self) {
    if (flash_heap_close(&self->heap) < 0) {
        mp_raise_OSError(errno);
    }
    freeze_last_ram_size += self->ram_end - self->ram_start;
}

static flash_ptr_t freeze_tell(freeze_writer_t *self) {
    if (!self->ram_pos) {
        return flash_heap_tell(&self->heap);
    }
    else {
        return (flash_ptr_t)self->ram_pos;
    }
}

#ifndef NDEBUG
static bool freeze_is_valid_ptr(freeze_writer_t *self, flash_ptr_t fptr) {
    return flash_heap_is_valid_ptr(&self->heap, fptr) || ((fptr >= (flash_ptr_t)self->ram_start) && (fptr <= (flash_ptr_t)self->ram_limit));
}
#endif

static flash_ptr_t freeze_seek(freeze_writer_t *self, flash_ptr_t fptr) {
    flash_ptr_t old_fptr = freeze_tell(self);
    if (flash_heap_is_valid_ptr(&self->heap, fptr)) {
        if (flash_heap_seek(&self->heap, fptr) < 0) {
            mp_raise_OSError(errno);
        }
        self->ram_pos = NULL;
    }
    else if((uint8_t *)fptr < self->ram_start) {
        mp_raise_OSError(MP_EINVAL);
    }
    else if ((uint8_t *)fptr > self->ram_limit) {
        mp_raise_OSError(MP_ENOSPC);
    }
    else {
        self->ram_end = MAX(self->ram_end, (uint8_t *)fptr);
        self->ram_pos = (uint8_t *)fptr;
    } 
    return old_fptr;
}

static void freeze_align(freeze_writer_t *self, size_t align) {
    flash_ptr_t fptr = freeze_tell(self);
    fptr = flash_heap_align(fptr, align); 
    freeze_seek(self, fptr);
}

static void freeze_read(freeze_writer_t *self, uint8_t *buffer, size_t length) {
    if (!self->ram_pos) {
        if (flash_heap_read(&self->heap, buffer, length) < 0) {
            mp_raise_OSError(errno);
        }
    }
    else if (self->ram_pos + length > self->ram_limit) {
        mp_raise_OSError(MP_ENOSPC);
    }
    else {
        memcpy(buffer, self->ram_pos, length);
        self->ram_pos += length;
    }
}

static void freeze_write(freeze_writer_t *self, const uint8_t *buffer, size_t length) {
    if (!self->ram_pos) {
        if (flash_heap_write(&self->heap, buffer, length) < 0) {
            mp_raise_OSError(errno);
        }
    }
    else if (self->ram_pos + length > self->ram_limit) {
        mp_raise_OSError(MP_ENOSPC);
    }    
    else {
        memcpy(self->ram_pos, buffer, length);
        self->ram_pos += length;
    }
}

static void freeze_write_char(freeze_writer_t *self, uint8_t value) {
    freeze_align(self, __alignof__(uint8_t));
    freeze_write(self, &value, sizeof(uint8_t));
}

static void freeze_write_short(freeze_writer_t *self, uint16_t value) {
    freeze_align(self, __alignof__(uint16_t));
    freeze_write(self, (uint8_t *)&value, sizeof(uint16_t));
}

static void freeze_write_int(freeze_writer_t *self, uint32_t value) {
    freeze_align(self, __alignof__(uint32_t));
    freeze_write(self, (uint8_t *)&value, sizeof(uint32_t));
}

static void freeze_write_size(freeze_writer_t *self, size_t value) {
    freeze_align(self, __alignof__(size_t));
    freeze_write(self, (uint8_t *)&value, sizeof(size_t));
}

static void freeze_write_intptr(freeze_writer_t *self, uintptr_t value) {
    freeze_align(self, __alignof__(uintptr_t));
    freeze_write(self, (uint8_t *)&value, sizeof(uintptr_t));
}

static bool freeze_is_freezable_ptr(freeze_writer_t *self, const void *ptr) {
    extern uint8_t end;
    return (ptr == 0) || 
        ((ptr >= (void *)XIP_BASE) && (ptr <= (void *)self->heap.flash_end)) || 
        ((ptr >= (void *)SRAM_BASE) && (ptr <= (void *)&end));
}

static void freeze_write_fptr(freeze_writer_t *self, flash_ptr_t fptr) {
    assert(freeze_is_valid_ptr(self, fptr) || freeze_is_freezable_ptr(self, (void *)fptr));
    freeze_write_intptr(self, (uintptr_t)fptr);
}

static flash_ptr_t freeze_allocate(freeze_writer_t *self, size_t size, size_t align, bool ram) {
    flash_ptr_t fptr = ram ? (flash_ptr_t)self->ram_end : self->heap.flash_end;
    fptr = flash_heap_align(fptr, align);
    if (!ram) {
        flash_ptr_t ret = flash_heap_tell(&self->heap);
        if (flash_heap_seek(&self->heap, fptr + size) < 0) {
            mp_raise_OSError(errno);
        }
        if (flash_heap_seek(&self->heap, ret) < 0) {
            mp_raise_OSError(errno);
        }
    }
    else if ((uint8_t *)fptr + size > self->ram_limit) {
        mp_raise_OSError(MP_ENOSPC);
    }
    else {
        self->ram_end = MAX(self->ram_end, (uint8_t *)fptr + size);
    } 
    return fptr;
}

static void freeze_add_ptr(freeze_writer_t *self, flash_ptr_t fptr, const void *ptr) {
    assert(freeze_is_valid_ptr(self, fptr));
    assert(!freeze_is_freezable_ptr(self, ptr));

    mp_map_elem_t *elem = mp_map_lookup(
        &self->obj_map,
        MP_OBJ_NEW_SMALL_INT((intptr_t)ptr),
        MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
    assert(elem->value == NULL);
    elem->value = MP_OBJ_NEW_SMALL_INT(fptr);
}

static bool freeze_lookup_ptr(freeze_writer_t *self, flash_ptr_t *fptr, const void *ptr) {
    if (freeze_is_freezable_ptr(self, ptr)) {
        *fptr = (flash_ptr_t)ptr;
        return true;
    }

    mp_map_elem_t *elem = mp_map_lookup(
        &self->obj_map,
        MP_OBJ_NEW_SMALL_INT((intptr_t)ptr),
        MP_MAP_LOOKUP);
    if (elem != NULL) {
        assert(elem->value != NULL);
        *fptr = MP_OBJ_SMALL_INT_VALUE(elem->value);
        return true;
    }

    return false;
}

typedef void (*freeze_write_t)(freeze_writer_t *self, const void *value);
typedef size_t (*freeze_sizeof_t)(const void *value);

static void freeze_write_ptr(freeze_writer_t *self, const void *ptr, size_t size, size_t align, bool ram) {
    if (freeze_is_freezable_ptr(self, ptr)) {
        freeze_write_fptr(self, (flash_ptr_t)ptr);
        return;
    }

    flash_ptr_t fptr = freeze_allocate(self, size, align, ram);
    flash_ptr_t ret = freeze_seek(self, fptr);
    freeze_write(self, ptr, size);
    freeze_seek(self, ret);

    freeze_write_fptr(self, fptr);
}

static void freeze_write_aliased_ptr(freeze_writer_t *self, const void *ptr, size_t size, size_t align, bool ram) {
    flash_ptr_t fptr;
    if (!freeze_lookup_ptr(self, &fptr, ptr)) {
        fptr = freeze_allocate(self, size, align, ram);
        freeze_add_ptr(self, fptr, ptr);
        flash_ptr_t ret = freeze_seek(self, fptr);
        freeze_write(self, ptr, size);
        freeze_seek(self, ret);
    }
    freeze_write_fptr(self, fptr);
}

static void freeze_write_obj(freeze_writer_t *self, mp_const_obj_t obj);

static void freeze_write_obj_array(freeze_writer_t *self, const mp_obj_t *objs, size_t count, bool ram) {
    flash_ptr_t fobjs = 0;
    if (objs) {
        fobjs = freeze_allocate(self, count * sizeof(mp_obj_t), __alignof__(mp_obj_t), ram);
        flash_ptr_t ret = freeze_seek(self, fobjs);
        for (int i = 0; i < count; i++) {
            freeze_write_obj(self, objs[i]);
        }
        freeze_seek(self, ret);
    }

    freeze_write_fptr(self, fobjs);
}

// ### base ###
static void freeze_write_raw_obj(freeze_writer_t *self, const mp_obj_base_t *raw_obj);

static void freeze_write_base(freeze_writer_t *self, const mp_obj_base_t *base) {
    freeze_align(self, __alignof__(mp_obj_base_t));
    freeze_write_raw_obj(self, &base->type->base);
}

// ### cell ###
extern const mp_obj_type_t mp_type_cell;

static void freeze_write_cell(freeze_writer_t *self, const mp_obj_cell_t *cell) {
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

static size_t freeze_sizeof_closure(const mp_obj_closure_t *closure) {
    return closure->n_closed * sizeof(mp_obj_t);
}

static void freeze_write_closure(freeze_writer_t *self, const mp_obj_closure_t *closure) {
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
static void freeze_write_map(freeze_writer_t *self, const mp_map_t *map, bool mutable) {
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

static void freeze_write_immutable_dict_ptr(freeze_writer_t *self, const mp_obj_dict_t *dict) {
    assert(dict->base.type == &mp_type_dict);

    flash_ptr_t fdict = freeze_allocate(self, sizeof(mp_obj_dict_t), __alignof__(mp_obj_dict_t), false);
    flash_ptr_t ret = freeze_seek(self, fdict);
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
static flash_ptr_t freeze_new_raw_code(freeze_writer_t *self, const mp_raw_code_t *rc) {
    flash_ptr_t frc;
    if (!freeze_lookup_ptr(self, &frc, rc)) {
        frc = freeze_allocate(self, sizeof(mp_raw_code_t), __alignof__(mp_raw_code_t), false);
        flash_ptr_t ret = freeze_seek(self, frc);
        freeze_add_ptr(self, frc, rc);

        freeze_write_int(self, *(mp_uint_t *)rc);

        freeze_write_ptr(self, rc->fun_data, rc->fun_data_len, __alignof__(char), false);
        
        flash_ptr_t fchild_table = 0;
        if (rc->children) {
            fchild_table = freeze_allocate(self, rc->n_children * sizeof(mp_raw_code_t *), __alignof__(mp_raw_code_t *), false);
            flash_ptr_t ret = freeze_seek(self, fchild_table);
            for (int i = 0; i < rc->n_children; i++) {
                flash_ptr_t fchild_rc = freeze_new_raw_code(self, rc->children[i]);
                freeze_write_fptr(self, fchild_rc);
            }
            freeze_seek(self, ret);
        }
        freeze_write_fptr(self, fchild_table);

        freeze_write_size(self, rc->fun_data_len);

        freeze_write_size(self, rc->n_children);

#if MICROPY_EMIT_MACHINE_CODE
        freeze_write_short(self, rc->prelude_offset);
#endif

#if MICROPY_PY_SYS_SETTRACE
        freeze_write_int(self, rc->line_of_definition);
        freeze_align(self, __alignof__(mp_bytecode_prelude_t));
        freeze_write(self, &rc->prelude, sizeof(mp_bytecode_prelude_t));
#endif

#if MICROPY_EMIT_INLINE_ASM
        freeze_write_int(self, rc->asm_n_pos_args | (rc->asm_type_sig << 8));
#endif

        freeze_seek(self, ret);
    }
    return frc;
}

static size_t freeze_sizeof_fun_bc(const mp_obj_fun_bc_t *fun_bc) {
    return fun_bc->n_extra_args * sizeof(mp_obj_t);
}

static void freeze_write_fun_bc(freeze_writer_t *self, const mp_obj_fun_bc_t *fun_bc) {
    assert((fun_bc->base.type == &mp_type_fun_bc) || (fun_bc->base.type == &mp_type_gen_wrap));

    freeze_align(self, __alignof__(mp_obj_fun_bc_t));
    freeze_write_base(self, &fun_bc->base);
    
    if (!fun_bc->context->module.base.type) {
        ((mp_module_context_t *)(fun_bc->context))->module.base.type = &mp_type_module;
    }
    freeze_write_raw_obj(self, &fun_bc->context->module.base);

    flash_ptr_t frc = freeze_new_raw_code(self, fun_bc->rc);
    mp_raw_code_t rc;
    if (flash_heap_is_valid_ptr(&self->heap, frc)) {
        flash_ptr_t ret = freeze_seek(self, frc);
        freeze_read(self, (uint8_t *)&rc, sizeof(mp_raw_code_t));
        freeze_seek(self, ret);
    }
    else {
        rc = *(mp_raw_code_t *)frc;
    }
    freeze_write_fptr(self, (flash_ptr_t)rc.children);
    freeze_write_fptr(self, (flash_ptr_t)rc.fun_data);
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

static void freeze_write_bound_meth(freeze_writer_t *self, const mp_obj_bound_meth_t *bound_meth) {
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

static void freeze_write_float_obj(freeze_writer_t *self, const mp_obj_float_t *float_obj) {
    assert(float_obj->base.type == &mp_type_float);
    freeze_align(self, __alignof__(mp_obj_float_t));
    freeze_write_base(self, &float_obj->base);
    freeze_align(self, __alignof__(mp_float_t));
    freeze_write(self, (uint8_t *)&float_obj->value, sizeof(mp_float_t));
}

// ### int ###
static void freeze_write_mpz(freeze_writer_t *self, const mpz_t *mpz) {
    freeze_align(self, __alignof__(mpz_t));
    freeze_write_size(self, *(size_t *)mpz);
    freeze_write_size(self, mpz->len);
    freeze_write_ptr(self, mpz->dig, mpz->len * sizeof(mpz_dig_t), __alignof__(mpz_dig_t), false);
}

static void freeze_write_int_obj(freeze_writer_t *self, const mp_obj_int_t *int_obj) {
    assert(int_obj->base.type == &mp_type_int);
    freeze_align(self, __alignof__(mp_obj_int_t));
    freeze_write_base(self, &int_obj->base);
    freeze_write_mpz(self, &int_obj->mpz);
}

// ### module ###
static void freeze_write_module(freeze_writer_t *self, const mp_obj_module_t *module) {
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

static void freeze_write_module_context(freeze_writer_t *self, const mp_module_context_t *context) {
    // printf("module_context: n_qstr=%d, n_obj=%d\n", context->constants.n_qstr, context->constants.n_obj);

    freeze_align(self, __alignof__(mp_module_context_t));
    freeze_write_module(self, &context->module);

    freeze_align(self, __alignof__(mp_module_constants_t));
    freeze_write_ptr(self, context->constants.qstr_table, context->constants.n_qstr * sizeof(qstr_short_t), __alignof__(qstr_short_t), false);
    freeze_write_obj_array(self, context->constants.obj_table, context->constants.n_obj, false);
    freeze_write_size(self, context->constants.n_qstr);
    freeze_write_size(self, context->constants.n_obj);
}

static flash_ptr_t freeze_new_module(freeze_writer_t *self, mp_obj_t module_obj) {
    const mp_module_context_t *module = MP_OBJ_TO_PTR(module_obj);
    flash_ptr_t fmodule;
    if (freeze_lookup_ptr(self, &fmodule, module)) {
        return fmodule;
    }

    fmodule = freeze_allocate(self, sizeof(mp_module_context_t), __alignof(mp_module_context_t), false);
    freeze_add_ptr(self, fmodule, module);
    flash_ptr_t ret = freeze_seek(self, fmodule);
    freeze_write_module_context(self, module);
    freeze_seek(self, ret);
    return fmodule;
}

static void freeze_write_non_frozen_module(freeze_writer_t *self, const mp_module_context_t *context) {
    mp_obj_t dest[2];
    mp_load_method_maybe(MP_OBJ_FROM_PTR(context), MP_QSTR___name__, dest);
    mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("module '%s' already loaded but not frozen"), mp_obj_str_get_str(dest[0]));
}

// ### property ###
typedef struct _mp_obj_property_t {
    mp_obj_base_t base;
    mp_obj_t proxy[3]; // getter, setter, deleter
} mp_obj_property_t;

static void freeze_write_property(freeze_writer_t *self, const mp_obj_property_t *property) {
    assert(property->base.type == &mp_type_property);

    freeze_align(self, __alignof__(mp_obj_property_t));
    freeze_write_base(self, &property->base);
    freeze_write_obj(self, property->proxy[0]);
    freeze_write_obj(self, property->proxy[1]);
    freeze_write_obj(self, property->proxy[2]);
}

// ### static/class method ###
static void freeze_write_static_class_method(freeze_writer_t *self, const mp_obj_static_class_method_t *scm) {
    assert((scm->base.type == &mp_type_staticmethod) || (scm->base.type == &mp_type_classmethod));

    freeze_align(self, __alignof__(mp_obj_static_class_method_t));
    freeze_write_base(self, &scm->base);
    freeze_write_obj(self, scm->fun);
}

// ### str ###
static void freeze_write_str(freeze_writer_t *self, const mp_obj_str_t *str) {
    assert((str->base.type == &mp_type_str) || (str->base.type == &mp_type_bytes));

    freeze_align(self, __alignof__(mp_obj_str_t));
    freeze_write_base(self, &str->base);
    freeze_write_size(self, str->hash);
    freeze_write_size(self, str->len);
    freeze_write_aliased_ptr(self, str->data, str->len + 1, __alignof__(char), false);
}

// ### tuple ###
static size_t freeze_sizeof_tuple(const mp_obj_tuple_t *tuple) {
    return tuple->len * sizeof(mp_obj_t);
}

static void freeze_write_tuple(freeze_writer_t *self, const mp_obj_tuple_t *tuple) {
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

static size_t freeze_sizeof_type(const mp_obj_type_t *type) {
    return freeze_type_num_slots(type) * sizeof(void *);
}

static void freeze_write_type(freeze_writer_t *self, const mp_obj_type_t *type) {
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
            freeze_write_fptr(self, (flash_ptr_t)type->slots[i]);
        }
    }
}

// ### instance ###
static size_t freeze_sizeof_instance(const mp_obj_instance_t *obj) {
    const mp_obj_type_t *native_base;
    size_t num_native_bases = instance_count_native_bases(obj->base.type, &native_base);
    return num_native_bases * sizeof(native_base);
}

static void freeze_write_instance(freeze_writer_t *self, const mp_obj_instance_t *obj) {
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
static void freeze_write_list(freeze_writer_t *self, const mp_obj_list_t *list) {
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

static void freeze_write_set(freeze_writer_t *self, const mp_obj_set_t *set) {
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

static size_t freeze_sizeof_re(const mp_obj_re_t *re) {
    return re->re.bytelen;
}

static void freeze_write_re(freeze_writer_t *self, const mp_obj_re_t *re) {
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
static struct freeze_type {
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

static const struct freeze_type *freeze_get_type(const mp_obj_type_t *type) { 
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

static flash_ptr_t freeze_new_raw_obj(freeze_writer_t *self, const mp_obj_base_t *raw_obj) {
    if (raw_obj == NULL) {
        return 0;
    }

    flash_ptr_t fraw_obj;
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
    flash_ptr_t ret = freeze_seek(self, fraw_obj);
    ftype->writer(self, raw_obj);
    freeze_seek(self, ret);
    return fraw_obj;
}

static void freeze_write_raw_obj(freeze_writer_t *self, const mp_obj_base_t *raw_obj) {
    flash_ptr_t fraw_obj = freeze_new_raw_obj(self, raw_obj);
    freeze_write_fptr(self, fraw_obj);
}

static void freeze_write_obj(freeze_writer_t *self, mp_const_obj_t obj) {
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
        flash_ptr_t fraw_obj = freeze_new_raw_obj(self, raw_obj);
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

static void freeze_write_qstr_pool(freeze_writer_t *self, const qstr_pool_t *pool, enum qstr_pool_field field) {
    if (freeze_is_freezable_ptr(self, pool)) {
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

static flash_ptr_t freeze_new_qstr_pool(freeze_writer_t *self, const qstr_pool_t *last_pool) {
    const qstr_pool_t *first_pool = last_pool;
    while (!freeze_is_freezable_ptr(self, first_pool) && first_pool->prev != NULL) {
        first_pool = first_pool->prev;
    }

    size_t total_prev_len = first_pool->total_prev_len + first_pool->len;
    size_t len = last_pool->total_prev_len + last_pool->len - total_prev_len;
    if (len == 0) {
        return 0;
    }

    flash_ptr_t fpool = freeze_allocate(self, sizeof(qstr_pool_t) + len * sizeof(uint8_t *), __alignof__(qstr_pool_t), false);
    flash_ptr_t fhashes = freeze_allocate(self, len * sizeof(qstr_hash_t), __alignof__(qstr_hash_t), false);
    flash_ptr_t flengths = freeze_allocate(self, len * sizeof(qstr_len_t), __alignof__(qstr_len_t), false);

    flash_ptr_t ret = freeze_seek(self, fpool);
    freeze_write_fptr(self, (flash_ptr_t)first_pool);
    freeze_write_size(self, total_prev_len);
    freeze_write_size(self, MIN(len, 10));
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
typedef struct {
    qstr module_name;
    const mp_obj_module_t *module;
    const void *ram_src;
    void *ram_dst;
    size_t ram_len;
} freeze_header_t;

bool freeze_clear() {
    if (freeze_mode > 0) {
        return false;
    }
    if (flash_heap_truncate(NULL) < 0) {
        mp_raise_OSError(errno);
    }
    freeze_mode = -1;
    return true;
}

void freeze_gc() {
    const flash_heap_header_t *header = NULL;
    while (flash_heap_iterate(&header) && (header < freeze_checkpoint)) {
        if (header->type == FREEZE_MODULE_FLASH_HEAP_TYPE) {
            const freeze_header_t *p = header->entry;
            gc_collect_root((void**)p->ram_dst, p->ram_len / sizeof(void *));            
        }
    }
}

static void freeze_set_qstr_pool(qstr_pool_t *qstr_pool) {
    MP_STATE_VM(last_pool) = qstr_pool;
    MP_STATE_VM(qstr_last_chunk) = NULL;
    MP_STATE_VM(qstr_last_alloc) = 0;
    MP_STATE_VM(qstr_last_used) = 0;
}

void freeze_init() {
    freeze_mode = 0;

    const flash_heap_header_t *header = NULL;
    while (flash_heap_iterate(&header)) {
        if (header->type == FREEZE_QSTR_POOL_FLASH_HEAP_TYPE) {
            const qstr_pool_t *qstr_pool = header->entry;
            assert(qstr_pool->prev == MP_STATE_VM(last_pool));
            freeze_set_qstr_pool((qstr_pool_t *)qstr_pool);
        }
        if (header->type == FREEZE_MODULE_FLASH_HEAP_TYPE) {
            const freeze_header_t *p = header->entry;
            freeze_last_ram_size += p->ram_len;
        }
    }
    freeze_checkpoint = flash_heap_next_header();
}

static bool freeze_check_module_name(mp_obj_t module_obj, qstr module_name) {
    if (module_obj == MP_OBJ_NULL) {
        return false;
    }
    mp_obj_t module_name_obj = mp_load_attr(module_obj, MP_QSTR___name__);
    return MP_OBJ_QSTR_VALUE(module_name_obj) == module_name;
}

mp_obj_t mp_module_get_frozen(qstr module_name, mp_obj_t outer_module_obj) {
    const flash_heap_header_t *header = NULL;
    while (flash_heap_iterate(&header) && (header < freeze_checkpoint)) {
        mp_obj_t module_obj;
        if (header->type == DL_FLASH_HEAP_TYPE) {
            mp_obj_t (*extmod_init)(void) = dlsym(header, "mp_extmod_init");
            if (!extmod_init) {
                continue;
            }
            module_obj = extmod_init();
            if (!freeze_check_module_name(module_obj, module_name)) {
                continue;
            }
        }
        else if (header->type == FREEZE_MODULE_FLASH_HEAP_TYPE) {
            const freeze_header_t *p = header->entry;
            if (p->module_name != module_name) {
                continue;
            }
            module_obj = MP_OBJ_FROM_PTR(p->module);
            memcpy(p->ram_dst, p->ram_src, p->ram_len);
        }
        else {
            continue;
        }

        mp_map_t *module_map = &MP_STATE_VM(mp_loaded_modules_dict).map;
        mp_map_elem_t *elem = mp_map_lookup(module_map, MP_OBJ_NEW_QSTR(module_name), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
        elem->value = module_obj;
        return module_obj;
    }
    return MP_OBJ_NULL;
}

mp_obj_t mp_module_freeze(qstr module_name, mp_obj_t module_obj, mp_obj_t outer_module_obj) {
    if (freeze_mode < 1) {
        return module_obj;
    }

    freeze_writer_t freezer;
    freeze_writer_init(&freezer, FREEZE_MODULE_FLASH_HEAP_TYPE);
    flash_ptr_t fmodule = freeze_new_module(&freezer, module_obj);
    
    size_t ram_size = freezer.ram_end - freezer.ram_start;
    flash_ptr_t ram_in_flash = freeze_allocate(&freezer, ram_size, 1, false);
    flash_heap_pwrite(&freezer.heap, freezer.ram_start, ram_size, ram_in_flash);

    flash_ptr_t fheader = freeze_allocate(&freezer, sizeof(freeze_header_t), __alignof__(freeze_header_t), false);
    freeze_seek(&freezer, fheader);
    freeze_write_int(&freezer, module_name);
    freeze_write_fptr(&freezer, fmodule);
    freeze_write_fptr(&freezer, ram_in_flash);
    freeze_write_fptr(&freezer, (flash_ptr_t)freezer.ram_start);
    freeze_write_size(&freezer, ram_size);
    
    freezer.heap.entry = fheader;
    freeze_writer_commit(&freezer);
    freeze_writer_deinit(&freezer);
    
    module_obj = (void *)fmodule;
    mp_map_t *mp_loaded_modules_map = &MP_STATE_VM(mp_loaded_modules_dict).map;
    mp_map_elem_t *elem = mp_map_lookup(mp_loaded_modules_map, MP_OBJ_NEW_QSTR(module_name), MP_MAP_LOOKUP_ADD_IF_NOT_FOUND);
    elem->value = module_obj;
    return module_obj;
}

static void freeze_qstrs(void) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        freeze_writer_t freezer;
        freeze_writer_init(&freezer, FREEZE_QSTR_POOL_FLASH_HEAP_TYPE);
        flash_ptr_t fpool = freeze_new_qstr_pool(&freezer, MP_STATE_VM(last_pool));
        if (fpool) {
            freezer.heap.entry = fpool;
            freeze_writer_commit(&freezer);
            freeze_set_qstr_pool((void *)fpool);
        }
        freeze_checkpoint = flash_heap_next_header();
        freeze_writer_deinit(&freezer);
        nlr_pop();
    }
    else {
        if (flash_heap_truncate(freeze_checkpoint) < 0) {
            panic("flash heap corrupted");
        }
        nlr_jump(nlr.ret_val);
    }
}

static void freeze_mode_nlr_callback(void *ctx) {
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
    size_t start_flash_size;
    size_t start_ram_size;
    size_t start_ram_size2 = freeze_last_ram_size;
    flash_heap_stats(&start_flash_size, &start_ram_size);
    nlr_jump_callback_node_t nlr_callback;
    nlr_push_jump_callback(&nlr_callback, freeze_mode_nlr_callback);
    for (size_t i = 0; i <n_args; i++) {
        items[i] = mp_builtin___import__(1, &args[i]);
    }
    freeze_qstrs();
    nlr_pop_jump_callback(true);

    size_t end_flash_size;
    size_t end_ram_size;
    flash_heap_stats(&end_flash_size, &end_ram_size);
    size_t end_ram_size2 = freeze_last_ram_size;
    mp_printf(&mp_plat_print, "froze %u flash bytes, %u ram bytes\n", end_flash_size - start_flash_size, end_ram_size - start_ram_size + end_ram_size2 - start_ram_size2);
    return result;
}

mp_obj_t freeze_modules(void) {
    mp_obj_t dict = mp_obj_new_dict(0);
    const flash_heap_header_t *header = NULL;
    while (flash_heap_iterate(&header) && (header < freeze_checkpoint)) {
        if (header->type == FREEZE_MODULE_FLASH_HEAP_TYPE) {
            const freeze_header_t *module_header = header->entry;
            mp_obj_t module_name = MP_OBJ_NEW_QSTR(module_header->module_name);
            mp_obj_t module_obj = MP_OBJ_FROM_PTR(module_header->module);
            mp_obj_dict_store(dict, module_name, module_obj);
        }
    }
    return dict;
}


// dynamic loader
typedef struct {
    const dl_linker_t *link_state;
    size_t num_qstrs;
    uint16_t *qstr_table;
} freeze_link_state_t;

// Machinery for running a C callback on the MP main thread
typedef int (*freeze_schedule_fun_t)(va_list args);

struct freeze_schedule_ctx {
    freeze_schedule_fun_t func;
    va_list args;
    int ret;
    TaskHandle_t task;
};

static mp_obj_t freeze_schedule_run(mp_obj_t arg) {
    struct freeze_schedule_ctx *ctx = (void *)MP_OBJ_SMALL_INT_VALUE(arg);
    ctx->ret = ctx->func(ctx->args);
    xTaskNotifyGive(ctx->task);
    return MP_OBJ_NULL;
}
static MP_DEFINE_CONST_FUN_OBJ_1(freeze_schedule_run_obj, freeze_schedule_run);

static int freeze_schedule(freeze_schedule_fun_t fun, ...) {
    va_list args;
    va_start(args, fun);
    if (mp_thread_get_state()) {
        // Already on the MP main thread, so just execute the function.
        int ret = fun(args);
        va_end(args);
        return ret;
    }

    struct freeze_schedule_ctx ctx = { fun, args, -1, xTaskGetCurrentTaskHandle() };
    // Verify context pointer fits into a MP small int. We cannot allocate a MP large int here.
    assert(MP_SMALL_INT_FITS((uintptr_t)&ctx));    
    xTaskNotifyStateClear(NULL);
    if (!mp_sched_schedule(MP_OBJ_FROM_PTR(&freeze_schedule_run_obj), MP_OBJ_NEW_SMALL_INT(&ctx))) {
        errno = ENOMEM;
        return -1;
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    va_end(args);
    return ctx.ret;
}

static int vfreeze_qstrs(va_list args) {
    freeze_qstrs();
    return 0;
}

static int freeze_post_link(const flash_heap_header_t *header) {
    return freeze_schedule(vfreeze_qstrs);
}

static int vqstr_from_strn(va_list args) {
    const char* str = va_arg(args, char *);
    size_t len = va_arg(args, size_t);
    return qstr_from_strn(str, len);
}

static int freeze_rewrite_obj(freeze_link_state_t *state, flash_ptr_t obj_addr);

__attribute__((used))
int ld_micropython(const dl_linker_t *link_state, dl_post_link_fun_t *post_link) {
    freeze_link_state_t state = { link_state, 0 };
    int result = -1;

    flash_ptr_t dyn_addr = 0;
    flash_ptr_t extmod_addr = 0;
    Elf32_Dyn dyn;
    while (dl_iterate_dynamic(link_state, &dyn_addr, &dyn) >= 0) {
        if (dyn.d_tag == DT_NULL) {
            break;
        }
        switch (dyn.d_tag) {
            case DT_LOOS+1:
                extmod_addr = dyn.d_un.d_ptr;
                break;
        }       
    }
    if (dyn_addr == 0) {
        return -1;
    }
    if (extmod_addr == 0) {
        return 0;
    }

    mp_extension_module_t extmod;
    if (dl_linker_read(link_state, &extmod, sizeof(extmod), extmod_addr) < 0) {
        goto cleanup;
    }
    state.num_qstrs = extmod.num_qstrs;
    state.qstr_table = dl_realloc(link_state, NULL, state.num_qstrs * sizeof(uint16_t));
    if (!state.qstr_table) {
        goto cleanup;
    }
    for (size_t i = 0; i < extmod.num_qstrs; i++) {
        const char *qstr;
        if (dl_linker_read(link_state, &qstr, sizeof(qstr), (flash_ptr_t)(extmod.qstrs + i)) < 0) {
            goto cleanup;
        }
        char str[256];
        int br = dl_linker_read(link_state, &str, sizeof(str), (flash_ptr_t)qstr);
        if (br < 0) {
            goto cleanup;
        }
        size_t len = strnlen(str, br);
        if (len == br) {
            str[len] = '\0';
            printf("qstr too long '%s...'\n", str);
            errno = EINVAL;
            goto cleanup;
        }
        uint16_t qid = freeze_schedule(vqstr_from_strn, str, len);
        state.qstr_table[i] = qid;
    }
    if (dl_linker_write(link_state, state.qstr_table, extmod.num_qstrs * sizeof(uint16_t), (flash_ptr_t)extmod.qstr_table) < 0) {
        goto cleanup;
    }
    for (const mp_rom_obj_t *obj = extmod.object_start; obj < extmod.object_end; obj++) {
        if (freeze_rewrite_obj(&state, (flash_ptr_t)obj) < 0) {
            goto cleanup;
        }
    }
    *post_link = freeze_post_link;
    result = 0;

cleanup:
    free(state.qstr_table);
    return result;
}

void freeze_flash(const char *file) {
    size_t start_flash_size;
    size_t start_ram_size;
    flash_heap_stats(&start_flash_size, &start_ram_size);    
    if (dl_flash(file) < 0) {
        mp_raise_OSError(errno);
    }
    freeze_qstrs();

    size_t end_flash_size;
    size_t end_ram_size;
    flash_heap_stats(&end_flash_size, &end_ram_size);
    mp_printf(&mp_plat_print, "froze %u flash bytes, %u ram bytes\n", end_flash_size - start_flash_size, end_ram_size - start_ram_size);
}


// ### dict ###
static int freeze_rewrite_map(freeze_link_state_t *state, const mp_map_t *map) {
    for (size_t i = 0; i < map->alloc; i++) {
        flash_ptr_t elem_addr = (flash_ptr_t)(map->table + i);
        mp_map_elem_t elem;
        if (dl_linker_read(state->link_state, &elem, sizeof(elem), elem_addr) < 0) {
            return -1;
        }
        assert(mp_obj_is_qstr(elem.key));
        if (freeze_rewrite_obj(state, elem_addr + offsetof(mp_map_elem_t, key)) < 0) {
            return -1;
        }
        if (mp_obj_is_qstr(elem.value) && freeze_rewrite_obj(state, elem_addr + offsetof(mp_map_elem_t, value)) < 0) {
            return -1;
        }
    }
    return 0;
}

static int freeze_rewrite_immutable_dict_ptr(freeze_link_state_t *state, flash_ptr_t dict_addr) {
    mp_obj_dict_t dict;
    if (dl_linker_read(state->link_state, &dict, sizeof(dict), dict_addr) < 0) {
        return -1;
    }    
    assert(dict.base.type == &mp_type_dict);
    return freeze_rewrite_map(state, &dict.map);
}

// ### module ###
static int freeze_rewrite_module(freeze_link_state_t *state, flash_ptr_t module_addr) {
    mp_obj_module_t module;
    if (dl_linker_read(state->link_state, &module, sizeof(module), module_addr) < 0) {
        return -1;
    }
    assert(module.base.type == &mp_type_module);

    return freeze_rewrite_immutable_dict_ptr(state, (flash_ptr_t)module.globals);
}

// ### type ###
static int freeze_rewrite_type(freeze_link_state_t *state, flash_ptr_t type_addr) {
    mp_obj_type_t type;
    if (dl_linker_read(state->link_state, &type, sizeof(type), type_addr) < 0) {
        return -1;
    }
    assert(type.base.type == &mp_type_type);

    if (mp_extmod_qstr(state->qstr_table, state->num_qstrs, &type.name) < 0) {
        return -1;
    }
    if (dl_linker_write(state->link_state, &type.name, sizeof(type.name), type_addr + offsetof(mp_obj_type_t, name)) < 0) {
        return -1;
    }

    if (type.slot_index_locals_dict) {
        flash_ptr_t slot_addr = type_addr + offsetof(mp_obj_type_t, slots) + (type.slot_index_locals_dict - 1) * sizeof(void *);
        const mp_obj_dict_t *locals_dict;
        if (dl_linker_read(state->link_state, &locals_dict, sizeof(locals_dict), slot_addr) < 0) {
            return -1;
        }
        return freeze_rewrite_immutable_dict_ptr(state, (flash_ptr_t)locals_dict);
    }
    return 0;
}

// ### qstr array ###
static int freeze_rewrite_qstr_array(freeze_link_state_t *state, flash_ptr_t qstr_obj_addr) {
    mp_obj_qstr_array_t qstr_obj;
    if (dl_linker_read(state->link_state, &qstr_obj, sizeof(qstr_obj), qstr_obj_addr) < 0) {
        return -1;
    }
    assert(qstr_obj.base.type == &mp_type_qstr_array);

    size_t num_elems = qstr_obj.array_size / qstr_obj.elem_size;
    for (size_t i = 0; i < num_elems; i++) {
        uint16_t qstr_short;
        flash_ptr_t qstr_addr = ((flash_ptr_t)qstr_obj.array) + i * qstr_obj.elem_size + qstr_obj.qstr_offset;
        if (dl_linker_read(state->link_state, &qstr_short, sizeof(uint16_t), qstr_addr) < 0) {
            return -1;
        }
        if (mp_extmod_qstr(state->qstr_table, state->num_qstrs, &qstr_short) < 0) {
            return -1;
        }
        if (dl_linker_write(state->link_state, &qstr_short, sizeof(uint16_t), qstr_addr) < 0) {
            return -1;
        }
    }
    return 0;
}

// ### raw_obj ###
static int freeze_rewrite_raw_obj(freeze_link_state_t *state, flash_ptr_t raw_obj_addr) {
    if (raw_obj_addr == 0) {
        return 0;
    }

    mp_obj_base_t base;
    if (dl_linker_read(state->link_state, &base, sizeof(base), raw_obj_addr) < 0) {
        return -1;
    }

    if (base.type == &mp_type_type) {
        return freeze_rewrite_type(state, raw_obj_addr);
    }
    else if (base.type == &mp_type_module) {
        return freeze_rewrite_module(state, raw_obj_addr);
    }
    else if (base.type == &mp_type_qstr_array) {
        return freeze_rewrite_qstr_array(state, raw_obj_addr);
    }
    else {
        printf("don't know how to refreeze type %p\n", base.type);
        errno = EINVAL;
        return -1;
    }
}

static int freeze_rewrite_obj(freeze_link_state_t *state, flash_ptr_t obj_addr) {
    mp_obj_t obj;
    if (dl_linker_read(state->link_state, &obj, sizeof(obj), obj_addr) < 0) {
        return -1;
    }
    if (mp_obj_is_qstr(obj)) {
        qstr_short_t qid = MP_OBJ_QSTR_VALUE(obj);
        if (mp_extmod_qstr(state->qstr_table, state->num_qstrs, &qid) < 0) {
            return -1;
        }
        obj = MP_OBJ_NEW_QSTR(qid);
        dl_linker_write(state->link_state, &obj, sizeof(obj), obj_addr);
    } else if (mp_obj_is_obj(obj)) {
        flash_ptr_t raw_obj_addr = (flash_ptr_t)MP_OBJ_TO_PTR(obj);
        return freeze_rewrite_raw_obj(state, raw_obj_addr);
    } 
    return 0;
}
