// SPDX-FileCopyrightText: 2024 Gregory Neverov
// SPDX-License-Identifier: MIT

#include <assert.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include "newlib/dlfcn.h"
#include "newlib/flash_heap.h"

#pragma GCC diagnostic ignored "-Wformat-truncation"


typedef char elf_str_t[256];

// Variable is properly initialized in dl_init.
static const flash_heap_header_t *dl_last_loaded = (void *)-1;

static int dl_error;
static char dl_error_msg[256];

struct dl_link_state {
    struct dl_loader_state *loader;
    // size_t num_segments;
    // Elf32_Phdr *segments;
    Elf32_Phdr pt_phdr;
    Elf32_Phdr pt_dynamic;
    Elf32_Phdr pt_interp;
    Elf32_Phdr pt_loos;

    flash_ptr_t dt_hash;
    flash_ptr_t dt_strtab;
    flash_ptr_t dt_symtab;
    flash_ptr_t dt_rela;
    Elf32_Word dt_relaent;
    Elf32_Word dt_relasz;
    // Elf32_Word dt_strsz;
    Elf32_Word dt_syment;
    flash_ptr_t dt_rel;
    Elf32_Word dt_relent;
    Elf32_Word dt_relsz;

    dl_post_link_fun_t post_link;
};

static void dl_seterror() {
    dl_error = errno;
}

__attribute__((visibility("hidden")))
int dl_loader_open(dl_loader_t *loader, uint device) {
    if (flash_heap_open(&loader->heap, device, DL_FLASH_HEAP_TYPE) < 0) {
        return -1;
    }
    if (loader->heap.flash_start < (flash_ptr_t)dl_last_loaded) {
        strcpy(dl_error_msg, "reset needed");
        errno = EPERM;
        return -1;
    }
    loader->flash_base = flash_heap_align(loader->heap.flash_pos, 8);
    loader->ram_base = flash_heap_align(loader->heap.ram_start, 8);

    dl_error = 0;
    memset(dl_error_msg, 0, sizeof(dl_error_msg));
    return 0;
}

__attribute__((visibility("hidden")))
void dl_loader_free(dl_loader_t *loader) {
    flash_heap_free(&loader->heap);
}

__attribute__((visibility("hidden")))
flash_ptr_t dl_loader_relocate(const dl_loader_t *loader, flash_ptr_t addr) {
    switch (addr >> 28) {
        case 1:
            return (addr & 0x0fffffff) + loader->flash_base;
        case 2:
            return (addr & 0x0fffffff) + loader->ram_base;
        default:
            errno = EFAULT;
            dl_seterror();
            return 0;
    }
}

__attribute__((visibility("hidden")))
int dl_loader_read(dl_loader_t *loader, void *buffer, size_t length, flash_ptr_t addr) {
    addr = dl_loader_relocate(loader, addr);
    if (!addr) {
        return -1;
    }
    int ret = flash_heap_pread(&loader->heap, buffer, length, addr);
    if (ret < 0) {
        dl_seterror();
    }
    return ret;
}

__attribute__((visibility("hidden")))
int dl_loader_write(dl_loader_t *loader, const void *buffer, size_t length, flash_ptr_t addr) {
    addr = dl_loader_relocate(loader, addr);
    if (!addr) {
        return -1;
    }
    int ret = flash_heap_pwrite(&loader->heap, buffer, length, addr);
    if (ret < 0) {
        dl_seterror();
    }
    return ret;
}

static int dl_loader_phdr(dl_loader_t *loader, Elf32_Phdr *phdr) {
    flash_ptr_t footer_addr = (loader->heap.flash_end - 8 - loader->flash_base) & ~255;
    uint32_t footer[2];
    if (flash_heap_pread(&loader->heap, footer, 8, loader->flash_base + footer_addr) < 0) {
        return -1;
    }
    if (~footer[0] != footer[1]) {
        strcpy(dl_error_msg, "bad footer");
        errno = ENOEXEC;
        return -1;
    }
    flash_ptr_t phdr_addr = dl_loader_relocate(loader, footer[0]);
    if (!phdr_addr) {
        snprintf(dl_error_msg, sizeof(dl_error_msg), "bad phdr addr 0x%08lx", footer[0]);
        return -1;
    }
    if (flash_heap_pread(&loader->heap, phdr, sizeof(Elf32_Phdr), phdr_addr) < 0) {
        return -1;
    }
    if (phdr->p_type != PT_PHDR) {
        snprintf(dl_error_msg, sizeof(dl_error_msg), "bad phdr type %lu", phdr->p_type);
        errno = ENOEXEC;
        return -1;
    }
    phdr->p_paddr = dl_loader_relocate(loader, phdr->p_paddr);
    phdr->p_vaddr = dl_loader_relocate(loader, phdr->p_vaddr);
    return 0;
}

static int copy_data(dl_loader_t *loader, int fd, size_t size, flash_ptr_t addr) {
    uint8_t buf[512];
    size_t remaining = size;
    while (remaining > 0) {
        int bw = read(fd, buf, MIN(512, remaining));
        if (bw == 0) {
            return -1;
        }
        if (dl_loader_write(loader, buf, bw, addr) < 0) {
            return -1;
        }
        addr += bw;
        remaining -= bw;
    }
    return size;
}

static int dl_linker_iterate_phdr(const dl_linker_t *linker, flash_ptr_t *phdr_addr, Elf32_Phdr *phdr) {
    assert(linker->pt_phdr.p_type == PT_PHDR);
    if (*phdr_addr == 0) {
        *phdr_addr = linker->pt_phdr.p_paddr;
    } else {
        *phdr_addr += sizeof(Elf32_Phdr);
    }
    if (*phdr_addr >= linker->pt_phdr.p_paddr + linker->pt_phdr.p_memsz) {
        return 0;
    }
    if (flash_heap_pread(&linker->loader->heap, phdr, sizeof(Elf32_Phdr), *phdr_addr) < 0) {
        return -1;
    }
    return 1;
}

__attribute__((visibility("hidden")))
flash_ptr_t dl_linker_map(const dl_linker_t *linker, flash_ptr_t addr) {
    flash_ptr_t phdr_addr = 0;
    Elf32_Phdr phdr;
    int ret = dl_linker_iterate_phdr(linker, &phdr_addr, &phdr);
    while (ret > 0) {
        flash_ptr_t offset = addr - phdr.p_vaddr;
        if ((phdr.p_type == PT_LOAD) && (offset < phdr.p_filesz)) {
            addr = phdr.p_paddr + offset;
            if (!flash_heap_is_valid_ptr(&linker->loader->heap, addr)) {
                goto error;
            }
            return addr;
        }
        ret = dl_linker_iterate_phdr(linker, &phdr_addr, &phdr);
    }
    if (ret < 0) {
        return ret;
    }
error:
    errno = EFAULT;
    dl_seterror();
    // snprintf(dl_error_msg, sizeof(dl_error_msg), "bad vaddr mapping %08x", addr);
    return 0;
}

__attribute__((visibility("hidden")))
int dl_iterate_dynamic(const dl_linker_t *linker, flash_ptr_t *dyn_addr, Elf32_Dyn *dyn) {
    assert(linker->pt_dynamic.p_type == PT_DYNAMIC);
    if (*dyn_addr == 0) {
        *dyn_addr = linker->pt_dynamic.p_paddr;
    } else {
        *dyn_addr += sizeof(Elf32_Dyn);
    }
    if (*dyn_addr >= linker->pt_dynamic.p_paddr + linker->pt_dynamic.p_memsz) {
        return 0;
    }
    if (flash_heap_pread(&linker->loader->heap, dyn, sizeof(Elf32_Dyn), *dyn_addr) < 0) {
        return -1;
    }
    return 1;
}

static int do_phdrs(dl_linker_t *linker);
static int do_dynamic(dl_linker_t *linker);
static int do_symtab(dl_linker_t *linker, size_t num_symbols);
static int do_rel(dl_linker_t *linker, flash_ptr_t rel, Elf32_Word relent, Elf32_Word relsz, bool a);
static int do_interp(dl_linker_t *linker);

__attribute__((visibility("hidden")))
int dl_link(dl_loader_t *loader) {
    dl_linker_t linker = { 0 };
    linker.loader = loader;

    if (do_phdrs(&linker) < 0) {
        goto error;
    }

    if ((linker.pt_dynamic.p_type == PT_DYNAMIC) && (do_dynamic(&linker) < 0)) {
        goto error;
    }

    size_t num_symbols = 0;
    if (linker.dt_hash) {
        Elf32_Word buckets[2];
        if (flash_heap_pread(&loader->heap, &buckets, sizeof(buckets), linker.dt_hash) < 0) {
            goto error;
        }
        num_symbols = buckets[1];
    }

    if (linker.dt_symtab && num_symbols && linker.dt_strtab && linker.dt_syment) {
        if (do_symtab(&linker, num_symbols) < 0) {
            goto error;
        }
        if (linker.dt_rela && linker.dt_relaent && linker.dt_relasz) {
            if (do_rel(&linker, linker.dt_rela, linker.dt_relaent, linker.dt_relasz, true) < 0) {
                goto error;
            }
        }
        if (linker.dt_rel && linker.dt_relent && linker.dt_relsz) {
            if (do_rel(&linker, linker.dt_rel, linker.dt_relent, linker.dt_relsz, false) < 0) {
                goto error;
            }
        }
    }

    if ((linker.pt_interp.p_type == PT_INTERP) && (do_interp(&linker) < 0)) {
        goto error;
    }

    if ((linker.pt_loos.p_type == PT_LOOS) && flash_heap_trim(&loader->heap, linker.pt_loos.p_paddr) < 0) {
        goto error;
    }

    loader->heap.entry = linker.pt_dynamic.p_vaddr;

    if (flash_heap_close(&loader->heap) < 0) {
        goto error;
    }

    if (linker.post_link && (linker.post_link(flash_heap_get_header(&loader->heap)) < 0)) {
        goto error;
    }
    return 0;

error:
    dl_seterror();
    return -1;
}

__attribute__((visibility("hidden")))
void *dl_load(const char *file, uint device) {
    dl_loader_t loader = { 0 };
    int fd = open(file, O_RDONLY, 0);
    if (fd < 0) {
        strcpy(dl_error_msg, "file error");
        goto cleanup;
    }

    size_t start_flash_size[FLASH_HEAP_NUM_DEVICES], start_ram_size;
    flash_heap_stats(start_flash_size, &start_ram_size);

    if (dl_loader_open(&loader, device) < 0) {
        goto cleanup;
    }

    Elf32_Ehdr ehdr;
    if (pread(fd, &ehdr, sizeof(ehdr), 0) < 0) {
        goto cleanup;
    }
    if ((memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) ||
        (ehdr.e_ident[EI_CLASS] != ELFCLASS32) ||
        (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) ||
        (ehdr.e_type != ET_EXEC) ||
        (ehdr.e_machine != EM_ARM)) {
        strcpy(dl_error_msg, "bad ELF header");
        errno = ENOEXEC;
        goto cleanup;
    }

    for (int i = 0; i < ehdr.e_phnum; i++) {
        Elf32_Phdr phdr;
        if (pread(fd, &phdr, sizeof(Elf32_Phdr), ehdr.e_phoff + i * ehdr.e_phentsize) < 0) {
            goto cleanup;
        }
        if (phdr.p_type != PT_LOAD) {
            continue;
        }
        if (lseek(fd, phdr.p_offset, SEEK_SET) < 0) {
            goto cleanup;
        }
        if (copy_data(&loader, fd, phdr.p_filesz, phdr.p_paddr) < 0) {
            goto cleanup;
        }
    }

    close(fd);
    fd = -1;

    if (dl_link(&loader) < 0) {
        goto cleanup;
    }

    dl_loader_free(&loader);
    size_t end_flash_size[FLASH_HEAP_NUM_DEVICES], end_ram_size;
    flash_heap_stats(end_flash_size, &end_ram_size);
    printf(
        "loaded %u flash bytes, %u ram bytes, %u psram bytes\n",
        end_flash_size[0] - start_flash_size[0],
        end_ram_size - start_ram_size,
        end_flash_size[1] - start_flash_size[1]);
    return (void *)loader.flash_base;

cleanup:
    dl_seterror();
    if (fd >= 0) {
        close(fd);
    }
    dl_loader_free(&loader);
    return NULL;
}

static int do_phdrs(dl_linker_t *linker) {
    dl_loader_t *loader = linker->loader;
    if (dl_loader_phdr(loader, &linker->pt_phdr) < 0) {
        return -1;
    }

    flash_ptr_t phdr_addr = 0;
    Elf32_Phdr phdr;
    int ret = dl_linker_iterate_phdr(linker, &phdr_addr, &phdr);
    while (ret > 0) {
        phdr.p_paddr = dl_loader_relocate(loader, phdr.p_paddr);
        if (!phdr.p_paddr) {
            return -1;
        }
        phdr.p_vaddr = dl_loader_relocate(loader, phdr.p_vaddr);
        if (!phdr.p_vaddr) {
            return -1;
        }
        switch (phdr.p_type) {
            case PT_LOAD:
                if (flash_heap_seek(&loader->heap, phdr.p_vaddr + phdr.p_memsz) < 0) {
                    return -1;
                }
                break;
            case PT_DYNAMIC:
                linker->pt_dynamic = phdr;
                break;
            case PT_INTERP:
                linker->pt_interp = phdr;
                break;
            case PT_LOOS:
                linker->pt_loos = phdr;
                break;
        }
        if (flash_heap_pwrite(&loader->heap, &phdr, sizeof(Elf32_Phdr), phdr_addr) < 0) {
            return -1;
        }
        ret = dl_linker_iterate_phdr(linker, &phdr_addr, &phdr);
    }
    return ret;
}

static int do_dynamic(dl_linker_t *linker) {
    dl_loader_t *loader = linker->loader;
    flash_ptr_t dyn_addr = 0;
    Elf32_Dyn dyn;
    int ret = dl_iterate_dynamic(linker, &dyn_addr, &dyn);
    while (ret > 0) {
        switch (dyn.d_tag) {
            case DT_HASH:
                dyn.d_un.d_ptr = linker->dt_hash = dl_loader_relocate(loader, dyn.d_un.d_ptr);
                break;

            case DT_STRTAB:
                dyn.d_un.d_ptr = linker->dt_strtab = dl_loader_relocate(loader, dyn.d_un.d_ptr);
                break;

            case DT_SYMTAB:
                dyn.d_un.d_ptr = linker->dt_symtab = dl_loader_relocate(loader, dyn.d_un.d_ptr);
                break;

            case DT_RELA:
                dyn.d_un.d_ptr = linker->dt_rela = dl_loader_relocate(loader, dyn.d_un.d_ptr);
                break;

            case DT_RELAENT:
                linker->dt_relaent = dyn.d_un.d_val;
                break;

            case DT_RELASZ:
                linker->dt_relasz = dyn.d_un.d_val;
                break;

            // case DT_STRSZ:
            //     strsz = dyn.d_un.d_val;
            //     break;

            case DT_SYMENT:
                linker->dt_syment = dyn.d_un.d_val;
                break;

            case DT_REL:
                dyn.d_un.d_ptr = linker->dt_rel = dl_loader_relocate(loader, dyn.d_un.d_ptr);
                break;

            case DT_RELENT:
                linker->dt_relent = dyn.d_un.d_val;
                break;

            case DT_RELSZ:
                linker->dt_relsz = dyn.d_un.d_val;
                break;

            case DT_PLTGOT:
            case DT_INIT:
            case DT_FINI:
            case DT_DEBUG:
            case DT_JMPREL:
            case DT_INIT_ARRAY:
            case DT_FINI_ARRAY:
                dyn.d_un.d_ptr = dl_loader_relocate(loader, dyn.d_un.d_ptr);
                break;

            default:
                if (!(dyn.d_tag % 2) && (dyn.d_tag > DT_ENCODING) && (dyn.d_tag <= DT_HIOS)) {
                    dyn.d_un.d_ptr = dl_loader_relocate(loader, dyn.d_un.d_ptr);
                }
                break;
        }
        if (flash_heap_pwrite(&loader->heap, &dyn, sizeof(dyn), dyn_addr) < 0) {
            return -1;
        }
        ret = dl_iterate_dynamic(linker, &dyn_addr, &dyn);
    }
    return ret;
}

static int do_symtab(dl_linker_t *state, size_t num_symbols) {
    dl_loader_t *loader = state->loader;
    for (int i = 1; i < num_symbols; i++) {
        flash_ptr_t sym_addr = state->dt_symtab + i * state->dt_syment;
        Elf32_Sym sym;
        if (flash_heap_pread(&loader->heap, &sym, sizeof(sym), sym_addr) < 0) {
            return -1;
        }

        if (sym.st_shndx == SHN_UNDEF) {
            elf_str_t sym_name;
            int br = flash_heap_pread(&loader->heap, sym_name, sizeof(sym_name), state->dt_strtab + sym.st_name);
            if (br < 0) {
                return -1;
            }
            if (strnlen(sym_name, br) == br) {
                sym_name[br] = '\0';
                snprintf(dl_error_msg, sizeof(dl_error_msg), "symbol name too long '%s...'", sym_name);
                errno = EINVAL;
                return -1;
            }

            sym.st_value = (Elf32_Addr)dlsym(NULL, sym_name);
            if (sym.st_value == 0) {
                snprintf(dl_error_msg, sizeof(dl_error_msg), "unresolved symbol '%s'", sym_name);
                errno = EINVAL;
                return -1;
            }
            sym.st_shndx = SHN_ABS;
        } else if (sym.st_shndx < SHN_LORESERVE) {
            Elf32_Addr sym_addr = sym.st_value;
            sym.st_value = dl_loader_relocate(loader, sym.st_value);
            if (!sym.st_value) {
                snprintf(dl_error_msg, sizeof(dl_error_msg), "bad symbol addr 0x%08lx", sym_addr);
                return -1;
            }
            sym.st_shndx = SHN_ABS;
        }
        if (flash_heap_pwrite(&loader->heap, &sym, sizeof(sym), sym_addr) < 0) {
            return -1;
        }
    }
    return 0;
}

static int do_rel(dl_linker_t *state, flash_ptr_t rel_addr, Elf32_Word relent, Elf32_Word relsz, bool a) {
    dl_loader_t *loader = state->loader;
    size_t num_relocs = relsz / relent;
    for (int i = 0; i < num_relocs; i++) {
        Elf32_Rela rel;
        if (flash_heap_pread(&loader->heap, &rel, a ? sizeof(Elf32_Rela) : sizeof(Elf32_Rel), rel_addr + i * relent) < 0) {
            return -1;
        }
        Elf32_Sym sym;
        if (flash_heap_pread(&loader->heap, &sym, sizeof(sym), state->dt_symtab + ELF32_R_SYM(rel.r_info) * state->dt_syment) < 0) {
            return -1;
        }
        Elf32_Addr S = sym.st_value;
        Elf32_Addr P = dl_loader_relocate(loader, rel.r_offset);
        if (!P) {
            snprintf(dl_error_msg, sizeof(dl_error_msg), "bad reloc addr 0x%08lx", rel.r_offset);
            return -1;
        }
        flash_ptr_t P_addr = dl_linker_map(state, dl_loader_relocate(loader, rel.r_offset));
        if (!P_addr) {
            snprintf(dl_error_msg, sizeof(dl_error_msg), "bad reloc addr 0x%08lx", rel.r_offset);
            return -1;
        }
        Elf32_Word insn;
        if (flash_heap_pread(&loader->heap, &insn, sizeof(Elf32_Word), P_addr) < 0) {
            return -1;
        }
        switch (ELF32_R_TYPE(rel.r_info)) {
            case R_ARM_ABS32:
            case R_ARM_TARGET1:
            case R_ARM_THM_PC22:
            case R_ARM_THM_JUMP24:
                break;
            default:
                snprintf(dl_error_msg, sizeof(dl_error_msg), "unsupported reloc type %ld", ELF32_R_TYPE(rel.r_info));
                errno = ENOSYS;
                return -1;
        }
        if (!a) {
            switch (ELF32_R_TYPE(rel.r_info)) {
                case R_ARM_ABS32:
                case R_ARM_TARGET1:
                    rel.r_addend = insn;
                    break;
                case R_ARM_THM_PC22:
                case R_ARM_THM_JUMP24:
                    // extract imm22 operand
                    rel.r_addend = ((insn & 0x7FF) << 11) | ((insn & 0x7FF0000) >> 16);
                    // sign extend
                    rel.r_addend = (rel.r_addend << 10) >> 10;
                    // convert unit to bytes
                    rel.r_addend <<= 1;
                    break;
            }
        }

        Elf32_Sword result;
        switch (ELF32_R_TYPE(rel.r_info)) {
            case R_ARM_ABS32:
            case R_ARM_TARGET1:
                result = S + rel.r_addend;
                break;
            case R_ARM_THM_PC22:
            case R_ARM_THM_JUMP24:
                result = S - P + rel.r_addend;
                break;
        }

        switch (ELF32_R_TYPE(rel.r_info)) {
            case R_ARM_ABS32:
            case R_ARM_TARGET1:
                insn = result;
                break;
            case R_ARM_THM_PC22:
            case R_ARM_THM_JUMP24:
                // convert unit to half words
                result >>= 1;
                // check for overflow
                assert(abs(result) < (1u << 21));
                // re-encode imm22 operand
                insn = (insn & 0xF800F800) | ((result & 0x3FF800) >> 11) | ((result & 0x7FF) << 16);
                break;
        }

        if (flash_heap_pwrite(&loader->heap, &insn, sizeof(Elf32_Word), P_addr) < 0) {
            return -1;
        }
    }
    return 0;
}

static int do_interp(dl_linker_t *linker) {
    dl_loader_t *loader = linker->loader;
    if (linker->pt_interp.p_memsz > sizeof(elf_str_t)) {
        strcpy(dl_error_msg, "interp name too long");
        errno = EINVAL;
        return -1;
    }
    elf_str_t interp = { 0 };
    if (flash_heap_pread(&loader->heap, interp, linker->pt_interp.p_memsz, linker->pt_interp.p_paddr) < 0) {
        return -1;
    }
    dl_link_fun_t interp_fun = dlsym(NULL, interp);
    if (!interp_fun) {
        snprintf(dl_error_msg, sizeof(dl_error_msg), "unsupported interp '%s'", interp);
        errno = EINVAL;
        return -1;
    }
    return interp_fun(linker, &linker->post_link);
}

__attribute__((visibility("hidden")))
int dl_linker_read(const dl_linker_t *linker, void *buffer, size_t length, flash_ptr_t addr) {
    addr = dl_linker_map(linker, addr);
    if (!addr) {
        return -1;
    }
    int ret = flash_heap_pread(&linker->loader->heap, buffer, length, addr);
    if (ret < 0) {
        dl_seterror();
    }
    return ret;
}

__attribute__((visibility("hidden")))
int dl_linker_write(const dl_linker_t *linker, const void *buffer, size_t length, flash_ptr_t addr) {
    addr = dl_linker_map(linker, addr);
    if (!addr) {
        return -1;
    }
    int ret = flash_heap_pwrite(&linker->loader->heap, buffer, length, addr);
    if (ret < 0) {
        dl_seterror();
    }
    return ret;
}

__attribute__((visibility("hidden")))
void *dl_realloc(const dl_linker_t *linker, void *ptr, size_t size) {
    return flash_heap_realloc_with_evict(&linker->loader->heap, ptr, size);
}


bool dl_iterate(const flash_heap_header_t **header) {
    bool result = flash_heap_iterate(0, header);
    while (result && ((*header)->type != DL_FLASH_HEAP_TYPE)) {
        result = flash_heap_iterate(0, header);
    }
    return result && (*header < dl_last_loaded);
}

void *dl_flash(const char *file) {
    return dl_load(file, 0);
}

void *dlopen(const char *file, int mode) {
    const flash_heap_header_t *header = NULL;
    while (dl_iterate(&header)) {
        const char *str = NULL;
        int soname = -1;
        for (const Elf32_Dyn *dyn = header->entry; dyn->d_tag; dyn++) {
            switch (dyn->d_tag) {
                case DT_STRTAB:
                    str = (char *)dyn->d_un.d_ptr;
                    break;
                case DT_SONAME:
                    soname = dyn->d_un.d_val;
                    break;
            }
        }
        if (str && (soname != -1) && strcmp(file, str + soname) == 0) {
            return (void *)header;
        }
    }
    strcpy(dl_error_msg, "file error");
    errno = ENOENT;
    dl_seterror();
    return NULL;
}

int dlclose(const void *handle) {
    return 0;
}

static void *dlsym_one(const flash_heap_header_t *header, const char *name) {
    const Elf32_Word *hash = NULL;
    const char *str = NULL;
    const Elf32_Sym *sym = NULL;
    for (const Elf32_Dyn *dyn = header->entry; dyn->d_tag; dyn++) {
        switch (dyn->d_tag) {
            case DT_HASH:
                hash = (Elf32_Word *)dyn->d_un.d_ptr;
                break;
            case DT_STRTAB:
                str = (char *)dyn->d_un.d_ptr;
                break;
            case DT_SYMTAB:
                sym = (Elf32_Sym *)dyn->d_un.d_ptr;
                break;
        }
    }
    if (!hash || !str || !sym) {
        return NULL;
    }
    size_t num_symbols = hash[1];
    for (int i = 0; i < num_symbols; i++) {
        if ((ELF32_ST_BIND(sym[i].st_info) != STB_LOCAL) &&
            (ELF32_ST_VISIBILITY(sym[i].st_other) == STV_DEFAULT) &&
            (strcmp(name, str + sym[i].st_name) == 0)) {
            return (void *)sym[i].st_value;
        }
    }
    return NULL;
}

void *dlsym(const void *handle, const char *name) {
    const flash_heap_header_t *header = handle;
    if (handle) {
        return dlsym_one(header, name);
    }
    while (dl_iterate(&header)) {
        void *result = dlsym_one(header, name);
        if (result) {
            return result;
        }
    }
    return NULL;
}

char *dlerror(void) {
    if (dl_error) {
        errno = dl_error;
        return dl_error_msg;
    }
    return NULL;
}

static const flash_heap_header_t *dl_finit(Elf32_Sword tag) {
    const flash_heap_header_t *header = NULL;
    while (dl_iterate(&header)) {
        const Elf32_Dyn *dyn = header->entry;
        while (dyn->d_tag != DT_NULL) {
            if (dyn->d_tag == tag) {
                ((void (*)(void))dyn->d_un.d_ptr)();
            }
            dyn++;
        }
    }
    return header;
}

__attribute__((constructor, visibility("hidden")))
void dl_init(void) {
    dl_last_loaded = dl_finit(DT_INIT);
}

__attribute__((destructor, visibility("hidden")))
void dl_fini(void) {
    dl_last_loaded = dl_finit(DT_FINI);
    dl_last_loaded = NULL;
}
