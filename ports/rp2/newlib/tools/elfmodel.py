# SPDX-FileCopyrightText: 2024 Gregory Neverov
# SPDX-License-Identifier: MIT

import bisect
import collections
from ctypes import alignment, sizeof
import elf as elf32
import itertools


def align(address, alignment):
    assert alignment > 0
    assert (alignment & (alignment - 1)) == 0
    return (address + alignment - 1) & ~(alignment - 1)


def e2s(enum, value):
    try:
        return enum(value).name[len(enum.__name__) + 1 :]
    except ValueError:
        return str(value)


class Node:
    fixed = False
    deleted = False

    def __init__(self, **kwargs):
        self.struct = type(self).c_type(**kwargs)

    @classmethod
    def readfrom(cls, fp):
        struct = cls.c_type()
        fp.readinto(struct)
        self = cls.__new__(cls, struct=struct)
        self.struct = struct
        self.fixed = True
        return self

    def delete(self):
        self.deleted = True

    def __str__(self):
        return str(self.struct)


class Symbol(Node):
    c_type = elf32.Sym

    def __init__(self, name, section=None, **kwargs):
        super().__init__(**kwargs)
        self.name = name
        self.section = section

    def __str__(self):
        return f"  {self.index:4}: {self.struct.st_value:08x} {self.struct.st_size:5} {e2s(elf32.STT, self.struct.st_type):7} {e2s(elf32.STB, self.struct.st_bind):6} {e2s(elf32.STV, self.struct.st_visibility):7} {e2s(elf32.SHN, self.struct.sec):4} {self.struct.name}"


class Relocation(Node):
    c_type = elf32.Rel

    def __init__(self, symbol, **kwargs):
        super().__init__(**kwargs)
        self.symbol = symbol


class RelocationWithAddend(Node):
    c_type = elf32.Rela

    def __init__(self, symbol, **kwargs):
        super().__init__(**kwargs)
        self.symbol = symbol


class Section(Node):
    c_type = elf32.Shdr

    sh_types = {}

    def __new__(cls, *args, struct=None, **kwargs):
        if cls is Section and struct is not None:
            cls = cls.sh_types.get(struct.sh_type, cls)
        return object.__new__(cls)

    def __init__(self, name, link=None, info=None, data=None, **kwargs):
        super().__init__(**kwargs)
        self.name = name
        self.link = link
        self.info = info
        self.data = data

    def delete(self):
        super().delete()
        # print(f"deleted section '{self.name}'")

    @property
    def size(self):
        return len(self.data) if self.data else self.struct.sh_size

    @property
    def psize(self):
        return 0 if self.struct.sh_type == elf32.SHT_NOBITS else self.size

    def __str__(self):
        s = f"  [{self.index:2}] {self.name:16} {e2s(elf32.SHT, self.struct.sh_type):16} {self.struct.sh_addr:08x} {self.struct.sh_offset:06x} {self.struct.sh_size:06x}"
        if hasattr(self, "paddr"):
            s += f" -- {self.paddr:08x}"
        return s


class EntrySection(Section):
    @property
    def _entries(self):
        if not hasattr(self, self._entries_name):
            setattr(self, self._entries_name, [])
        return getattr(self, self._entries_name)

    @_entries.setter
    def _entries(self, value):
        setattr(self, self._entries_name, value)

    def delete(self):
        super().delete()
        for e in self._entries:
            e.delete()

    @property
    def size(self):
        return len(self._entries) * self._entry_size


class SymtabSection(EntrySection):
    _entries_name = "symbols"
    _entry_size = sizeof(elf32.Sym)
    _entry_class = Symbol

    def __init__(self, name, link, sh_type=elf32.SHT_SYMTAB, **kwargs):
        super().__init__(
            name,
            link,
            None,
            sh_type=sh_type,
            sh_addralign=alignment(elf32.Sym),
            sh_entsize=sizeof(elf32.Sym),
            **kwargs,
        )
        self.symbols = []
        self.symbols.append(Symbol(name=None))

    def get_all_symbols(self, name):
        return [st for st in self.symbols if st.name == name]

    def get_first_symbol(self, name):
        syms = self.get_all_symbols(name)
        return syms[0] if syms else None


Section.sh_types[elf32.SHT_SYMTAB] = SymtabSection
Section.sh_types[elf32.SHT_DYNSYM] = SymtabSection


class RelSection(EntrySection):
    _entries_name = "relocs"
    _entry_size = sizeof(elf32.Rel)
    _entry_class = Relocation

    def __init__(self, name, link, info, **kwargs):
        super().__init__(
            name,
            link,
            info,
            sh_type=elf32.SHT_REL,
            sh_addralign=alignment(elf32.Rel),
            sh_entsize=sizeof(elf32.Rel),
            **kwargs,
        )
        self.relocs = []


Section.sh_types[elf32.SHT_REL] = RelSection


class RelaSection(EntrySection):
    _entries_name = "relocs"
    _entry_size = sizeof(elf32.Rela)
    _entry_class = RelocationWithAddend

    def __init__(self, name, link, info, **kwargs):
        super().__init__(
            name,
            link,
            info,
            sh_type=elf32.SHT_RELA,
            sh_addralign=alignment(elf32.Rela),
            sh_entsize=sizeof(elf32.Rela),
            **kwargs,
        )
        self.relocs = []


Section.sh_types[elf32.SHT_RELA] = RelaSection


class StrtabSection(Section):
    def __init__(self, name, **kwargs):
        super().__init__(name, sh_type=elf32.SHT_STRTAB, sh_addralign=1, **kwargs)
        self.strings = collections.OrderedDict()

    def register(self, s):
        assert isinstance(s, str)
        if s != "":
            self.data = None
            self.strings[s] = None

    def lookup(self, index):
        end_index = self.data.index(b"\0", index)
        return self.data[index:end_index].decode()

    def build(self):
        d = []
        key_fn = lambda kv: kv[0]
        for s in self.strings:
            rev_s = s[::-1]
            i = bisect.bisect_left(d, rev_s, key=key_fn)
            if i < len(d):
                key, value = d[i]
                if key.startswith(rev_s):
                    bisect.insort_right(value, s, key=len)
                    continue
            if i > 0:
                key, value = d[i - 1]
                if rev_s.startswith(key):
                    bisect.insort_right(value, s, key=len)
                    d[i - 1] = (rev_s, value)
                    continue
            d.insert(i, (rev_s, [s]))

        self.data = bytearray(b"\0")
        self.strings.clear()
        self.strings[""] = 0
        for key, value in d:
            self.data.extend(value[-1].encode())
            for s in value:
                self.strings[s] = len(self.data) - len(s)
            self.data.extend(b"\0")
        for key, value in self.strings.items():
            assert key == self.lookup(value)


Section.sh_types[elf32.SHT_STRTAB] = StrtabSection


class DynEntry(Node):
    c_type = elf32.Dyn

    def __init__(self, value=None, **kwargs):
        super().__init__(**kwargs)
        self.value = value


class DynamicSection(EntrySection):
    _entries_name = "dyns"
    _entry_size = sizeof(elf32.Dyn)
    _entry_class = DynEntry

    def __init__(self, name, link, **kwargs):
        super().__init__(
            name,
            link,
            None,
            sh_type=elf32.SHT_DYNAMIC,
            sh_addralign=alignment(elf32.Dyn),
            sh_entsize=sizeof(elf32.Dyn),
            **kwargs,
        )
        self.dyns = []


Section.sh_types[elf32.SHT_DYNAMIC] = DynamicSection


class EhdrSection(Section):
    """Pseudo-section that refers to the ELF file header"""

    def __init__(self, name=".ehdr", **kwargs):
        super().__init__(name, sh_addralign=alignment(elf32.Ehdr), **kwargs)


class PhdrsSection(Section):
    """Pseudo-section that refers to the program headers"""

    def __init__(self, name=".phdrs", **kwargs):
        super().__init__(name, sh_addralign=alignment(elf32.Phdr), **kwargs)


class Segment(Node):
    c_type = elf32.Phdr

    def __init__(self, sections=None, **kwargs):
        super().__init__(**kwargs)
        self.sections = [] if sections is None else sections

    def __contains__(self, section):
        if not isinstance(section, Section):
            raise TypeError()
        return (
            self.struct.p_vaddr <= section.struct.sh_addr
            and self.struct.p_vaddr + self.struct.p_memsz
            >= section.struct.sh_addr + section.struct.sh_size
        )

    def __str__(self):
        return f"  {e2s(elf32.PT, self.struct.p_type):12} 0x{self.struct.p_offset:06x} 0x{self.struct.p_vaddr:08x} 0x{self.struct.p_paddr:08x} 0x{self.struct.p_filesz:06x} 0x{self.struct.p_memsz:06x}"


class Elf(Node):
    c_type = elf32.Ehdr

    def __init__(self, **kwargs):
        super.__init__(
            e_ehsize=sizeof(elf32.Ehdr),
            e_phentsize=sizeof(elf32.Phdr),
            e_shentsize=sizeof(elf32.Shdr),
            **kwargs,
        )
        self.segments = []
        self.sections = []
        self.shstrtab = StrtabSection(".shstrtab")

    def iter_sections(self, sh_type):
        return (sh for sh in self.sections if sh.struct.sh_type == sh_type)


class Visitor:
    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    def visit(self, node, *args):
        if isinstance(node, list):
            for n in node:
                if not n.deleted:
                    self.visit(n, *args)
            return None

        if not isinstance(node, Node):
            raise TypeError(f"unrecognized node type '{type(node).__name__}'")
        assert not node.deleted

        node_type = type(node)
        while node_type != object:
            method = f"visit_{node_type.__name__}"
            if hasattr(type(self), method):
                try:
                    return getattr(type(self), method)(self, node, *args)
                except:
                    print(f"While processing {node_type.__name__} node:")
                    print(str(node))
                    raise
            node_type = node_type.__bases__[0]

        print(type(node))
        assert 0

    def visit_Elf(self, elffile, *args):
        self.visit(elffile.sections, elffile, *args)
        self.visit(elffile.segments, elffile, *args)

    def visit_Section(self, section, *args):
        pass

    def visit_EhdrSection(self, section, *args):
        pass

    def visit_PhdrsSection(self, section, *args):
        pass

    def visit_Segment(self, segment, *args):
        pass

    def visit_EntrySection(self, section, *args):
        self.visit_Section(section, *args)
        self.visit(section._entries, section, *args)

    def visit_Symbol(self, symbol, *args):
        pass

    def visit_Relocation(self, reloc, *args):
        pass

    def visit_RelocationWithAddend(self, reloc, *args):
        pass

    def visit_DynEntry(self, dyn, *args):
        pass


class PurgeDeleted(Visitor):
    """Transitively removes all deleted nodes"""

    def visit_Elf(self, elffile):
        self.again = True
        while self.again:
            self.again = False
            super().visit_Elf(elffile)
            elffile.segments = [p for p in elffile.segments if not p.deleted]
            elffile.sections = [sh for sh in elffile.sections if not sh.deleted]

    def visit_Segment(self, segment, *args):
        segment.sections = [sh for sh in segment.sections if not sh.deleted]
        if not segment.sections:
            self.delete(segment)

    def visit_Section(self, section, *args):
        if (section.link and section.link.deleted) or (section.info and section.info.deleted):
            self.delete(section)

    def visit_EntrySection(self, section, *args):
        super().visit_EntrySection(section, *args)
        section._entries = [x for x in section._entries if not x.deleted]

    def visit_Symbol(self, sym, *args):
        if sym.section and sym.section.deleted:
            self.delete(sym)

    def visit_Relocation(self, rel, *args):
        if rel.symbol and rel.symbol.deleted:
            self.delete(rel)

    def visit_RelocationWithAddend(self, rel, *args):
        if rel.symbol and rel.symbol.deleted:
            self.delete(rel)

    def delete(self, node):
        node.delete()
        self.again = True


class IndexNodes(Visitor):
    def visit_Elf(self, elffile):
        index = itertools.count(0)
        self.visit(elffile.sections, index)

        elffile.struct.e_phnum = len(elffile.segments)
        elffile.struct.e_shnum = next(index)

    def visit_Section(self, section, index):
        section.index = next(index)

    def visit_SymtabSection(self, section, index):
        self.visit_Section(section, index)
        local_symbols = [st for st in section.symbols if st.struct.st_bind == elf32.STB_LOCAL]
        nonlocal_symbols = [st for st in section.symbols if st.struct.st_bind != elf32.STB_LOCAL]
        local_symbols.sort(key=lambda st: st.struct.st_value)
        nonlocal_symbols.sort(key=lambda st: st.struct.st_value)
        section.symbols = local_symbols + nonlocal_symbols
        section.struct.sh_info = len(local_symbols)
        self.visit(section.symbols, itertools.count(0))

    def visit_RelSection(self, section, index):
        self.visit_Section(section, index)
        section.relocs.sort(key=lambda r: r.struct.r_offset)
        self.visit(section.relocs)

    def visit_RelaSection(self, section, index):
        self.visit_RelSection(section, index)

    def visit_Symbol(self, sym, index):
        sym.index = next(index)


class RegisterStrings(Visitor):
    def visit_Section(self, section, elffile):
        elffile.shstrtab.register(section.name)

    def visit_Symbol(self, sym, symtab, *args):
        if sym.name:
            symtab.link.register(sym.name)

    def visit_DynEntry(self, dyn, dynamic, *args):
        if isinstance(dyn.value, str):
            dynamic.link.register(dyn.value)


class BuildStrtabs(Visitor):
    def visit_StrtabSection(self, section, *args):
        if section.data is None:
            section.build()


class ComputeAddresses(Visitor):
    def __init__(self):
        self.next_flash = 0x10000000
        self.next_ram = 0x20000000

    def visit_Elf(self, elffile, *args):
        self.visit(elffile.segments, elffile, *args)
        self.visit(elffile.sections, elffile, *args)

    def visit_Segment(self, segment, elffile):
        if not segment.fixed:
            return

        delta = segment.struct.p_paddr - segment.struct.p_vaddr
        for sh in segment.sections:
            sh.paddr = sh.struct.sh_addr + delta

    def visit_Section(self, section, elffile):
        section.struct.sh_size = section.size
        if not section.struct.sh_flags & elf32.SHF_ALLOC:
            return

        if section.struct.sh_flags & elf32.SHF_WRITE:
            if not section.fixed:
                section.struct.sh_addr = align(self.next_ram, section.struct.sh_addralign)
            self.next_ram = section.struct.sh_addr + align(
                section.size, section.struct.sh_addralign
            )

        if (
            not section.struct.sh_flags & elf32.SHF_WRITE
            or section.struct.sh_type != elf32.SHT_NOBITS
        ):
            if not section.fixed:
                section.paddr = align(self.next_flash, section.struct.sh_addralign)
            self.next_flash = section.paddr + align(section.psize, section.struct.sh_addralign)

        if not section.fixed:
            if not section.struct.sh_flags & elf32.SHF_WRITE:
                section.struct.sh_addr = section.paddr
            elif section.struct.sh_type == elf32.SHT_NOBITS:
                section.paddr = section.struct.sh_addr

    def visit_PhdrsSection(self, section, elffile):
        section.struct.sh_size = elffile.struct.e_phnum * elffile.struct.e_phentsize
        self.visit_Section(section, elffile)

    def visit_EhdrSection(self, section, elffile):
        section.struct.sh_size = elffile.struct.e_ehsize
        self.visit_Section(section, elffile)


class ComputeOffsets(Visitor):
    def __init__(self, sh_align=4096):
        self.sh_align = sh_align
        self.offset = 0

    def align(self, alignment):
        self.offset = align(self.offset, alignment)
        return self.offset

    def visit_Elf(self, elffile):
        self.offset = elffile.struct.e_ehsize
        elffile.struct.e_phoff = self.align(alignment(elf32.Phdr))
        self.offset += elffile.struct.e_phnum * elffile.struct.e_phentsize
        self.offset = self.align(self.sh_align)

        self.visit(elffile.sections, elffile)
        elffile.struct.e_shoff = self.align(alignment(elf32.Shdr))
        self.offset += elffile.struct.e_shnum * elffile.struct.e_shentsize

    def visit_Section(self, section, elffile):
        if section.struct.sh_type != elf32.SHT_NULL:
            section.struct.sh_offset = self.align(section.struct.sh_addralign)
            self.offset += section.psize

    def visit_PhdrsSection(self, section, elffile):
        section.struct.sh_offset = elffile.struct.e_phoff

    def visit_EhdrSection(self, section, elffile):
        section.struct.sh_offset = 0


class ComputeSegments(Visitor):
    def visit_Segment(self, segment, elffile):
        section0 = segment.sections[0]
        segment.struct.p_offset = section0.struct.sh_offset
        if not segment.fixed:
            segment.struct.p_vaddr = section0.struct.sh_addr
            segment.struct.p_paddr = section0.paddr
            segment.struct.p_filesz = 0
            segment.struct.p_memsz = 0
            segment.struct.p_flags = elf32.PF_R
            segment.struct.p_align = 1

        for section in segment.sections:
            if not segment.fixed:
                assert (
                    section.struct.sh_offset >= segment.struct.p_offset + segment.struct.p_filesz
                )
                segment.struct.p_filesz = (
                    section.struct.sh_offset + section.psize - segment.struct.p_offset
                )

                assert section.paddr >= segment.struct.p_paddr + segment.struct.p_memsz

                assert section.struct.sh_addr >= segment.struct.p_vaddr + segment.struct.p_memsz
                segment.struct.p_memsz = (
                    section.struct.sh_addr + section.size - segment.struct.p_vaddr
                )

                if section.struct.sh_flags & elf32.SHF_WRITE:
                    segment.struct.p_flags |= elf32.PF_W
                if section.struct.sh_flags & elf32.SHF_EXECINSTR:
                    segment.struct.p_flags |= elf32.PF_X

                segment.struct.p_align = max(segment.struct.p_align, section.struct.sh_addralign)
            else:
                assert section.fixed


class WriteData(Visitor):
    def __init__(self, fp):
        self.fp = fp

    def visit_Section(self, section, *args):
        if section.struct.sh_type != elf32.SHT_NULL:
            self.fp.seek(section.struct.sh_offset)
            if section.struct.sh_type != elf32.SHT_NOBITS:
                self.fp.write(section.data)

    def visit_EntrySection(self, section, *args):
        self.fp.seek(section.struct.sh_offset)
        self.visit(section._entries, section)

    def visit_Symbol(self, sym, symtab):
        if sym.name:
            sym.struct.st_name = symtab.link.strings[sym.name]
        if sym.section:
            sym.struct.st_shndx = sym.section.index
        self.fp.write(sym.struct)

    def visit_Relocation(self, rel, reltab):
        rel.struct.r_sym = rel.symbol.index
        self.fp.write(rel.struct)

    def visit_RelocationWithAddend(self, rel, reltab):
        rel.struct.r_sym = rel.symbol.index
        self.fp.write(rel.struct)

    def visit_DynEntry(self, dyn, dynamic):
        if callable(dyn.value):
            dyn.struct.d_val = dyn.value()
        elif isinstance(dyn.value, str):
            dyn.struct.d_val = dynamic.link.strings[dyn.value]
        else:
            assert dyn.value is None
        self.fp.write(dyn.struct)


class WriteHeaders(Visitor):
    def __init__(self, fp):
        self.fp = fp

    def visit_Elf(self, elffile):
        self.fp.seek(0)
        elffile.struct.e_shstrndx = elffile.shstrtab.index
        self.fp.write(elffile.struct)

        self.fp.seek(elffile.struct.e_phoff)
        self.visit(elffile.segments)

        self.fp.seek(elffile.struct.e_shoff)
        self.visit(elffile.sections, elffile)

    def visit_Segment(self, segment):
        self.fp.write(segment.struct)

    def visit_Section(self, section, elffile):
        if section.name is not None:
            section.struct.sh_name = elffile.shstrtab.strings[section.name]
        if section.link is not None:
            section.struct.sh_link = section.link.index
        if section.info is not None:
            section.struct.sh_flags |= elf32.SHF_INFO_LINK
            section.struct.sh_info = section.info.index
        self.fp.write(section.struct)


def load(fp):
    fp.seek(0)
    elffile = Elf.readfrom(fp)

    fp.seek(elffile.struct.e_phoff)
    elffile.segments = []
    for i in range(elffile.struct.e_phnum):
        segment = Segment.readfrom(fp)
        elffile.segments.append(segment)

    fp.seek(elffile.struct.e_shoff)
    elffile.sections = []
    for i in range(elffile.struct.e_shnum):
        section = Section.readfrom(fp)
        elffile.sections.append(section)

    return elffile


class ReadData(Visitor):
    def visit_Section(self, section, *args):
        self.fp.seek(section.struct.sh_offset)
        section.data = (
            self.fp.read(section.struct.sh_size)
            if section.struct.sh_type != elf32.SHT_NOBITS
            else None
        )

    def visit_EntrySection(self, section, *args):
        self.fp.seek(section.struct.sh_offset)
        num_entries = section.struct.sh_size // section.struct.sh_entsize
        for i in range(num_entries):
            entry = section._entry_class.readfrom(self.fp)
            section._entries.append(entry)

    def visit_StrtabSection(self, section, *args):
        self.visit_Section(section)
        section.strings = collections.OrderedDict()


class Dereference(Visitor):
    def visit_Elf(self, elffile):
        elffile.shstrtab = elffile.sections[elffile.struct.e_shstrndx]
        super().visit_Elf(elffile)

    def visit_Segment(self, segment, elffile):
        segment.sections = [sh for sh in elffile.sections if sh in segment]

    def visit_Section(self, section, elffile):
        section.name = elffile.shstrtab.lookup(section.struct.sh_name)
        section.link = elffile.sections[section.struct.sh_link]
        section.info = None
        if section.struct.sh_flags & elf32.SHF_INFO_LINK:
            section.info = elffile.sections[section.struct.sh_info]

    def visit_EntrySection(self, section, elffile):
        self.visit_Section(section, elffile)
        self.visit(section._entries, section, elffile)

    def visit_Symbol(self, sym, symtab, elffile):
        sym.name = symtab.link.lookup(sym.struct.st_name)
        sym.section = (
            elffile.sections[sym.struct.st_shndx]
            if elf32.SHN_UNDEF < sym.struct.st_shndx < elf32.SHN_LORESERVE
            else None
        )

    def visit_Relocation(self, rel, reltab, elffile):
        rel.symbol = reltab.link.symbols[rel.struct.r_sym]


def open_elffile(path):
    with open(path, "rb") as fp:
        elffile = load(fp)
        ReadData(fp=fp).visit(elffile)
        Dereference().visit(elffile)
        return elffile


class Dump(Visitor):
    def visit_Segment(self, segment, elffile):
        print(str(segment))

    def visit_Section(self, section, elffile):
        print(str(section))
