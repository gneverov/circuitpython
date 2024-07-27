# SPDX-FileCopyrightText: 2024 Gregory Neverov
# SPDX-License-Identifier: MIT

import argparse
import elf as elf32
from elfmodel import *
import os


parser = argparse.ArgumentParser(
    description="Processes an extension module ELF file for dynamic linking"
)
parser.add_argument("input", help="input ELF file")
parser.add_argument("--output", help="output ELF file")
parser.add_argument(
    "--entry",
    nargs=2,
    metavar=("tag", "symbol"),
    dest="dyn_entries",
    action="append",
    help="add symbol as dynamic entry",
)
args = parser.parse_args()

module_name = args.input
elffile = open_elffile(module_name)

for section in elffile.sections:
    # if section.name.startswith(".debug"):
    #     section.delete()
    if section.name.startswith(".data"):
        section.struct.sh_flags |= elf32.SHF_WRITE
    if section.name.startswith(".uninitialized_data"):
        section.struct.sh_flags |= elf32.SHF_ALLOC
PurgeDeleted().visit(elffile)

dynstr = StrtabSection(".dynstr", sh_flags=elf32.SHF_ALLOC)
dynsym = SymtabSection(".dynsym", link=dynstr, sh_type=elf32.SHT_DYNSYM, sh_flags=elf32.SHF_ALLOC)
dynhash = Section(
    ".hash",
    link=dynsym,
    sh_type=elf32.SHT_HASH,
    sh_flags=elf32.SHF_ALLOC,
    sh_addralign=sizeof(elf32.Word),
)
dynrela = RelaSection(".rela.dyn", link=dynsym, info=None, sh_flags=elf32.SHF_ALLOC)
dynrel = RelSection(".rel.dyn", link=dynsym, info=None, sh_flags=elf32.SHF_ALLOC)
dynamic = DynamicSection(".dynamic", link=dynstr, sh_flags=elf32.SHF_ALLOC)

ehdr = EhdrSection(sh_flags=elf32.SHF_ALLOC)
phdrs = PhdrsSection(sh_flags=elf32.SHF_ALLOC)
interp = Section(
    ".interp",
    sh_type=elf32.SHT_PROGBITS,
    sh_flags=elf32.SHF_ALLOC,
    sh_addralign=1,
    data="ld_micropython".encode(),
)
footer = Section(
    ".footer", sh_type=elf32.SHT_PROGBITS, sh_flags=elf32.SHF_ALLOC, sh_addralign=256, sh_size=8
)


def mk_dyn(sym):
    if not hasattr(sym, "dyn"):
        dsym = Symbol(
            sym.name,
            sym.section,
            st_value=sym.struct.st_value,
            st_size=sym.struct.st_size,
            st_info=sym.struct.st_info,
            st_other=sym.struct.st_other,
            st_shndx=sym.struct.st_shndx if sym.section else elf32.SHN_UNDEF,
        )
        dynsym.symbols.append(dsym)
        sym.dyn = dsym
    return sym.dyn


for section in elffile.iter_sections(elf32.SHT_SYMTAB):
    for sym in section.symbols:
        if not sym.section:
            continue
        if sym.struct.st_bind != elf32.STB_LOCAL and sym.struct.st_visibility == elf32.STV_DEFAULT:
            mk_dyn(sym)
        if sym.name.startswith("__") and sym.name.endswith("_veneer"):
            # GCC with -q does not emit relocations within generated veneer functions. Therefore we
            # create our own such relocations here. The symbol for the real function is assumed to
            # be at offset 12 within the veneer.
            r_offset = (sym.struct.st_value & 0xFFFFFFFE) + 12
            offset = r_offset - sym.section.struct.sh_addr
            real_sym_value = int.from_bytes(sym.section.data[offset : offset + 4], "little")

            real_sym_name = sym.name[2:-7]
            real_sym = [
                x
                for x in section.get_all_symbols(real_sym_name)
                if x.struct.st_value == real_sym_value
            ]
            if not real_sym:
                raise ValueError(
                    f"cannot find symbol '{real_sym_name}' for veneer '{sym.name}' with value 0x{real_sym_value:08x}"
                )

            dynrela.relocs.append(
                RelocationWithAddend(
                    mk_dyn(real_sym[0]),
                    r_offset=r_offset,
                    r_type=elf32.R_ARM.R_ARM_ABS32,
                    r_addend=0,
                )
            )


def decode_addend(r_type, insn):
    if r_type in (elf32.R_ARM.R_ARM_ABS32, elf32.R_ARM.R_ARM_TARGET1):
        pass

    elif r_type == elf32.R_ARM.R_ARM_PREL31:
        # extract 31-bit signed int
        insn &= 0x7FFFFFFF
        # sign extend
        if insn & 0x40000000:
            insn -= 0x80000000

    elif r_type == elf32.R_ARM.R_ARM_THM_PC22:
        # extract imm22 operand
        insn = ((insn & 0x7FF) << 11) | ((insn & 0x7FF0000) >> 16)
        # sign extend
        if insn & 0x200000:
            insn -= 0x400000
        #  convert unit to bytes
        insn *= 2

    else:
        raise ValueError()

    return insn


def undo_relocation(r_type, S, P, A):
    if r_type in (elf32.R_ARM.R_ARM_ABS32, elf32.R_ARM.R_ARM_TARGET1):
        return A - S
    elif r_type in (elf32.R_ARM.R_ARM_THM_PC22, elf32.R_ARM.R_ARM_PREL31):
        S &= -2
        return A - (S - P)
    else:
        raise ValueError()


for section in elffile.iter_sections(elf32.SHT_REL):
    if not (section.info.struct.sh_flags & elf32.SHF_ALLOC):
        section.delete()
        continue
    for rel in section.relocs:
        sym = rel.symbol
        if sym.struct.st_shndx == elf32.STN_UNDEF:
            continue
        if (rel.struct.r_type == elf32.R_ARM.R_ARM_THM_PC22) and (sym.struct.st_value >> 28) != (
            rel.struct.r_offset >> 28
        ):
            veneer_sym = [st for st in section.link.symbols if st.name == f"__{sym.name}_veneer"]
            if not veneer_sym:
                raise ValueError(f"missing veneer for '{sym.name}'")
            sym = veneer_sym[0]

        if rel.struct.r_type not in (
            elf32.R_ARM.R_ARM_ABS32,
            elf32.R_ARM.R_ARM_TARGET1,
            elf32.R_ARM.R_ARM_THM_PC22,
            elf32.R_ARM.R_ARM_PREL31,
        ):
            sym_name = sym.section.name if sym.struct.st_type == elf32.STT_SECTION else sym.name
            raise ValueError(
                f"unsupported relocation type {elf32.R_ARM(rel.struct.r_type)._name_} of symbol '{sym_name}' in section '{section.name}'"
            )

        offset = rel.struct.r_offset - section.info.struct.sh_addr
        insn = int.from_bytes(section.info.data[offset : offset + 4], "little")
        addend = decode_addend(rel.struct.r_type, insn)
        addend = undo_relocation(
            rel.struct.r_type, sym.struct.st_value, rel.struct.r_offset, addend
        )
        if sym.section:
            if rel.struct.r_type in (elf32.R_ARM.R_ARM_ABS32, elf32.R_ARM.R_ARM_TARGET1):
                dynrela.relocs.append(
                    RelocationWithAddend(
                        mk_dyn(sym),
                        r_offset=rel.struct.r_offset,
                        r_info=rel.struct.r_info,
                        r_addend=addend,
                    )
                )
            elif rel.struct.r_type in (elf32.R_ARM.R_ARM_THM_PC22, elf32.R_ARM.R_ARM_PREL31):
                assert (sym.struct.st_value >> 28) == (rel.struct.r_offset >> 28)
                pass
            else:
                raise ValueError()
        else:
            if rel.struct.r_type == elf32.R_ARM.R_ARM_THM_PC22:
                assert abs(addend) < 0x00400000
            dynrela.relocs.append(
                RelocationWithAddend(
                    mk_dyn(sym),
                    r_offset=rel.struct.r_offset,
                    r_info=rel.struct.r_info,
                    r_addend=addend,
                )
            )

    section.delete()


dynamic.dyns.append(DynEntry(d_tag=elf32.DT_HASH, value=lambda: dynhash.struct.sh_addr))
dynhash.data = int.to_bytes(0, sizeof(elf32.Word), "little") + int.to_bytes(
    len(dynsym.symbols), sizeof(elf32.Word), "little"
)

dynamic.dyns.append(DynEntry(d_tag=elf32.DT_STRTAB, value=lambda: dynstr.struct.sh_addr))

dynamic.dyns.append(DynEntry(d_tag=elf32.DT_SYMTAB, value=lambda: dynsym.struct.sh_addr))

if len(dynrela.relocs):
    dynamic.dyns.append(DynEntry(d_tag=elf32.DT_RELA, value=lambda: dynrela.struct.sh_addr))

    dynamic.dyns.append(DynEntry(d_tag=elf32.DT_RELAENT, d_ptr=dynrela.struct.sh_entsize))

    dynamic.dyns.append(DynEntry(d_tag=elf32.DT_RELASZ, value=lambda: dynrela.size))

dynamic.dyns.append(DynEntry(d_tag=elf32.DT_STRSZ, value=lambda: dynstr.size))

dynamic.dyns.append(DynEntry(d_tag=elf32.DT_SYMENT, d_val=dynsym.struct.sh_entsize))

dynamic.dyns.append(DynEntry(d_tag=elf32.DT_SONAME, value=os.path.basename(module_name)))

if len(dynrel.relocs):
    dynamic.dyns.append(DynEntry(d_tag=elf32.DT_REL, value=lambda: dynrel.struct.sh_addr))

    dynamic.dyns.append(DynEntry(d_tag=elf32.DT_RELENT, d_ptr=dynrel.struct.sh_entsize))

    dynamic.dyns.append(DynEntry(d_tag=elf32.DT_RELSZ, value=lambda: dynrel.size))

dynamic.dyns.append(DynEntry(d_tag=elf32.DT_FLAGS, d_val=elf32.DF_BIND_NOW | elf32.DF_TEXTREL))

init = dynsym.get_first_symbol("__dl_init")
if init:
    dynamic.dyns.append(DynEntry(d_tag=elf32.DT_INIT, d_ptr=init.struct.st_value))

fini = dynsym.get_first_symbol("__dl_fini")
if fini:
    dynamic.dyns.append(DynEntry(d_tag=elf32.DT_FINI, d_ptr=fini.struct.st_value))

for tag, symbol_name in args.dyn_entries:
    tag = int(tag, 16)
    symbol = dynsym.get_first_symbol(symbol_name)
    if symbol:
        dynamic.dyns.append(DynEntry(d_tag=tag, d_ptr=symbol.struct.st_value))

dynamic.dyns.append(DynEntry())

elffile.sections.extend([phdrs, dynamic, dynhash, dynstr, dynsym, interp, dynrela, dynrel, footer])

phdrs_segment = Segment(p_type=elf32.PT_PHDR, sections=[phdrs])
interp_segment = Segment(p_type=elf32.PT_INTERP, sections=[interp])
elffile.segments[0:0] = [phdrs_segment, interp_segment]

phdrs_load_segment = Segment(p_type=elf32.PT_LOAD, sections=[phdrs])
dynamic_segment = Segment(p_type=elf32.PT_DYNAMIC, sections=[dynamic])
dyn_load_segment = Segment(
    p_type=elf32.PT_LOAD, sections=[dynamic, dynhash, dynstr, dynsym, interp, dynrela, dynrel]
)
loos_segment = Segment(p_type=elf32.PT_LOOS, sections=[dynrela, dynrel])
footer_load_segment = Segment(p_type=elf32.PT_LOAD, sections=[footer])
elffile.segments.extend(
    [phdrs_load_segment, dynamic_segment, dyn_load_segment, loos_segment, footer_load_segment]
)


IndexNodes().visit(elffile)
RegisterStrings().visit(elffile)
BuildStrtabs().visit(elffile)
ComputeAddresses().visit(elffile)
ComputeOffsets().visit(elffile)
ComputeSegments().visit(elffile)


phdr_addr = phdrs.paddr
footer.data = phdr_addr.to_bytes(4, "little") + (~phdr_addr & 0xFFFFFFFF).to_bytes(4, "little")


out_module_name = args.output or module_name
fp = open(out_module_name, "wb")
WriteData(fp=fp).visit(elffile)
WriteHeaders(fp=fp).visit(elffile)
fp.close()
