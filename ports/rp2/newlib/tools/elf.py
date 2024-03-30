# SPDX-FileCopyrightText: 2024 Gregory Neverov
# SPDX-License-Identifier: MIT

from ctypes import *
from enum import *


# Type for a 16-bit quantity.
Half = c_uint16

# Types for signed and unsigned 32-bit quantities.
Word = c_uint32
Sword = c_int32

# Types for signed and unsigned 64-bit quantities.
Xword = c_uint64
Sxword = c_int64

# Type of addresses.
Addr = c_uint32

# Type of file offsets.
Off = c_uint32

# Type for section indices, which are 16-bit quantities.
Section = c_uint16

# Type for version symbol information.
Versym = Half

EI_NIDENT = 16


# The ELF file header.  This appears at the start of every ELF file.
class Ehdr(Structure):
    _fields_ = [
        # Magic number and other info
        ("e_ident", c_ubyte * EI_NIDENT),
        # Object file type
        ("e_type", Half),
        # Architecture
        ("e_machine", Half),
        # Object file version
        ("e_version", Word),
        # Entry point virtual address
        ("e_entry", Addr),
        # Program header table file offset
        ("e_phoff", Off),
        # Section header table file offset
        ("e_shoff", Off),
        # Processor-specific flags
        ("e_flags", Word),
        # ELF header size in bytes
        ("e_ehsize", Half),
        # Program header table entry size
        ("e_phentsize", Half),
        # Program header table entry coun
        ("e_phnum", Half),
        # Section header table entry size
        ("e_shentsize", Half),
        # Section header table entry count
        ("e_shnum", Half),
        # Section header string table index
        ("e_shstrndx", Half),
    ]


# Section header.
class Shdr(Structure):
    _fields_ = [
        # Section name (string tbl index)
        ("sh_name", Word),
        # Section type
        ("sh_type", Word),
        # Section flags
        ("sh_flags", Word),
        # Section virtual addr at execution
        ("sh_addr", Addr),
        # Section file offset
        ("sh_offset", Off),
        # Section size in bytes
        ("sh_size", Word),
        # Link to another section
        ("sh_link", Word),
        # Additional section information
        ("sh_info", Word),
        # Section alignment
        ("sh_addralign", Word),
        # Entry size if section holds table
        ("sh_entsize", Word),
    ]


def global_enum(e):
    globals().update(e.__members__)
    return e


# Special section indices.
@global_enum
class SHN(IntEnum):
    # Undefined section
    SHN_UNDEF = 0

    # Start of reserved indices
    SHN_LORESERVE = 0xFF00

    # Start of processor-specific
    SHN_LOPROC = 0xFF00

    # End of processor-specific
    SHN_HIPROC = 0xFF1F

    # Start of OS-specific
    SHN_LOOS = 0xFF20

    # End of OS-specific
    SHN_HIOS = 0xFF3F

    # Associated symbol is absolute
    SHN_ABS = 0xFFF1

    # Associated symbol is common
    SHN_COMMON = 0xFFF2

    # End of reserved indices
    SHN_HIRESERVE = 0xFFFF


# Legal values for sh_type (section type).
@global_enum
class SHT(IntEnum):
    # Section header table entry unused
    SHT_NULL = 0

    # Program data
    SHT_PROGBITS = 1

    # Symbol table
    SHT_SYMTAB = 2

    # String table
    SHT_STRTAB = 3

    # Relocation entries with addends
    SHT_RELA = 4

    # Symbol hash table
    SHT_HASH = 5

    # Dynamic linking information
    SHT_DYNAMIC = 6

    # Notes
    SHT_NOTE = 7

    # Program space with no data (bss)
    SHT_NOBITS = 8

    # Relocation entries, no addends
    SHT_REL = 9

    # Reserved
    SHT_SHLIB = 10

    # Dynamic linker symbol table
    SHT_DYNSYM = 11

    # Array of constructors
    SHT_INIT_ARRAY = 14

    # Array of destructors
    SHT_FINI_ARRAY = 15

    # Array of pre-constructors
    SHT_PREINIT_ARRAY = 16

    # Section group
    SHT_GROUP = 17

    # Extended section indices
    SHT_SYMTAB_SHNDX = 18

    # Number of defined types.
    SHT_NUM = 19

    # Start OS-specific.
    SHT_LOOS = 0x60000000

    # End OS-specific type
    SHT_HIOS = 0x6FFFFFFF

    # Start of processor-specific
    SHT_LOPROC = 0x70000000

    # End of processor-specific
    SHT_HIPROC = 0x7FFFFFFF

    # Start of application-specific
    SHT_LOUSER = 0x80000000

    # End of application-specific
    SHT_HIUSER = 0x8FFFFFFF


# Legal values for sh_flags (section flags).
@global_enum
class SHF(IntEnum):
    # Writable
    SHF_WRITE = 1 << 0

    # Occupies memory during execution
    SHF_ALLOC = 1 << 1

    # Executable
    SHF_EXECINSTR = 1 << 2

    # Might be merged
    SHF_MERGE = 1 << 4

    # Contains nul-terminated strings
    SHF_STRINGS = 1 << 5

    # `sh_info' contains SHT index
    SHF_INFO_LINK = 1 << 6

    # Preserve order after combining
    SHF_LINK_ORDER = 1 << 7

    # Non-standard OS specific handling required
    SHF_OS_NONCONFORMING = 1 << 8

    # Section is member of a group.
    SHF_GROUP = 1 << 9

    # Section hold thread-local data.
    SHF_TLS = 1 << 10

    # Section with compressed data.
    SHF_COMPRESSED = 1 << 11

    # OS-specific.
    SHF_MASKOS = 0x0FF00000

    # Processor-specific
    SHF_MASKPROC = 0xF0000000


# How to extract and insert information held in the st_info field.
def ST_BIND(info):
    return info >> 4


def ST_TYPE(info):
    return info & 0xF


def ST_INFO(bind, type):
    return c_ubyte((bind << 4) | (type & 0xF))


# How to extract and insert information held in the st_other field.
def ST_VISIBILITY(other):
    return other & 0x03


# Symbol table entry.
class Sym(Structure):
    _fields_ = [
        # Symbol name (string tbl index)
        ("st_name", Word),
        # Symbol value
        ("st_value", Addr),
        # Symbol size
        ("st_size", Word),
        # Symbol type and binding
        ("st_info", c_ubyte),
        # Symbol visibility
        ("st_other", c_ubyte),
        # Section index
        ("st_shndx", Section),
    ]

    @property
    def st_bind(self):
        return ST_BIND(self.st_info)

    @st_bind.setter
    def st_bind(self, value):
        self.st_info = ST_INFO(value, self.st_type)

    @property
    def st_type(self):
        return ST_TYPE(self.st_info)

    @st_type.setter
    def st_type(self, value):
        self.st_info = ST_INFO(self.st_bind, value)

    @property
    def st_visibility(self):
        return ST_VISIBILITY(self.st_other)


# Legal values for ST_BIND subfield of st_info (symbol binding).
@global_enum
class STB(IntEnum):
    # Local symbol
    STB_LOCAL = 0

    # Global symbol
    STB_GLOBAL = 1

    # Weak symbol
    STB_WEAK = 2

    # Number of defined types.
    STB_NUM = 3

    # Start of OS-specific
    STB_LOOS = 10

    # End of OS-specific
    STB_HIOS = 12

    # Start of processor-specific
    STB_LOPROC = 13

    # End of processor-specific
    STB_HIPROC = 15


# Legal values for ST_TYPE subfield of st_info (symbol type).
@global_enum
class STT(IntEnum):
    # Symbol type is unspecified
    STT_NOTYPE = 0

    # Symbol is a data object
    STT_OBJECT = 1

    # Symbol is a code object
    STT_FUNC = 2

    # Symbol associated with a section
    STT_SECTION = 3

    # Symbol's name is file name
    STT_FILE = 4

    # Symbol is a common data object
    STT_COMMON = 5

    # Symbol is thread-local data object
    STT_TLS = 6

    # Number of defined types.
    STT_NUM = 7

    # Start of OS-specific
    STT_LOOS = 10

    # End of OS-specific
    STT_HIOS = 12

    # Start of processor-specific
    STT_LOPROC = 13

    # End of processor-specific
    STT_HIPROC = 15


# Symbol table indices are found in the hash buckets and chain table
# of a symbol hash table section.  This special index value indicates
# the end of a chain, meaning no further symbols are found in that bucket.
@global_enum
class STN(IntEnum):
    # End of a chain.
    STN_UNDEF = 0


# Symbol visibility specification encoded in the st_other field.
@global_enum
class STV(IntEnum):
    # Default symbol visibility rules
    STV_DEFAULT = 0

    # Processor specific hidden class
    STV_INTERNAL = 1

    # Sym unavailable in other modules
    STV_HIDDEN = 2

    # Not preemptible, not exported
    STV_PROTECTED = 3


# How to extract and insert information held in the r_info field.
def R_SYM(info):
    return info >> 8


def R_TYPE(info):
    return info & 0xFF


def R_INFO(sym, type):
    return (sym << 8) | (type & 0xFF)


# Relocation table entry without addend (in section of type SHT_REL).
class Rel(Structure):
    _fields_ = [
        # Address
        ("r_offset", Addr),
        # Relocation type and symbol index
        ("r_info", Word),
    ]

    @property
    def r_sym(self):
        return R_SYM(self.r_info)

    @r_sym.setter
    def r_sym(self, value):
        self.r_info = R_INFO(value, self.r_type)

    @property
    def r_type(self):
        return R_TYPE(self.r_info)

    @r_type.setter
    def r_type(self, value):
        self.r_info = R_INFO(self.r_sym, value)


# Relocation table entry with addend (in section of type SHT_RELA).
class Rela(Structure):
    _fields_ = [
        # Address
        ("r_offset", Addr),
        # Relocation type and symbol index
        ("r_info", Word),
        # Addend
        ("r_addend", Sword),
    ]

    @property
    def r_sym(self):
        return R_SYM(self.r_info)

    @r_sym.setter
    def r_sym(self, value):
        self.r_info = R_INFO(value, self.r_type)

    @property
    def r_type(self):
        return R_TYPE(self.r_info)

    @r_type.setter
    def r_type(self, value):
        self.r_info = R_INFO(self.r_sym, value)


# ARM relocs.
class R_ARM(IntEnum):
    # No reloc
    R_ARM_NONE = 0

    # Deprecated PC relative 26 bit branch.
    R_ARM_PC24 = 1

    # Direct 32 bit
    R_ARM_ABS32 = 2

    # PC relative 32 bit
    R_ARM_REL32 = 3

    R_ARM_PC13 = 4

    # Direct 16 bit
    R_ARM_ABS16 = 5

    # Direct 12 bit
    R_ARM_ABS12 = 6

    # Direct & 0x7C (LDR, STR).
    R_ARM_THM_ABS5 = 7

    # Direct 8 bit
    R_ARM_ABS8 = 8

    R_ARM_SBREL32 = 9

    # PC relative 24 bit (Thumb32 BL).
    R_ARM_THM_PC22 = 10

    # PC relative & 0x3FC (Thumb16 LDR, ADD, ADR).
    R_ARM_THM_PC8 = 11

    R_ARM_AMP_VCALL9 = 12

    # Obsolete static relocation.
    R_ARM_SWI24 = 13

    # Dynamic relocation.
    R_ARM_TLS_DESC = 13

    # Reserved.
    R_ARM_THM_SWI8 = 14

    # Reserved.
    R_ARM_XPC25 = 15

    # Reserved.
    R_ARM_THM_XPC22 = 16

    # ID of module containing symbol
    R_ARM_TLS_DTPMOD32 = 17

    # Offset in TLS block
    R_ARM_TLS_DTPOFF32 = 18

    # Offset in static TLS block
    R_ARM_TLS_TPOFF32 = 19

    # Copy symbol at runtime
    R_ARM_COPY = 20

    # Create GOT entry
    R_ARM_GLOB_DAT = 21

    # Create PLT entry
    R_ARM_JUMP_SLOT = 22

    # Adjust by program base
    R_ARM_RELATIVE = 23

    # 32 bit offset to GOT
    R_ARM_GOTOFF = 24

    # 32 bit PC relative offset to GOT
    R_ARM_GOTPC = 25

    # 32 bit GOT entry
    R_ARM_GOT32 = 26

    # Deprecated, 32 bit PLT address.
    R_ARM_PLT32 = 27

    # PC relative 24 bit (BL, BLX).
    R_ARM_CALL = 28

    # PC relative 24 bit (B, BL<cond>).
    R_ARM_JUMP24 = 29

    # PC relative 24 bit (Thumb32 B.W).
    R_ARM_THM_JUMP24 = 30

    # Adjust by program base.
    R_ARM_BASE_ABS = 31

    # Obsolete.
    R_ARM_ALU_PCREL_7_0 = 32

    # Obsolete.
    R_ARM_ALU_PCREL_15_8 = 33

    # Obsolete.
    R_ARM_ALU_PCREL_23_15 = 34

    # Deprecated, prog. base relative.
    R_ARM_LDR_SBREL_11_0 = 35

    # Deprecated, prog. base relative.
    R_ARM_ALU_SBREL_19_12 = 36

    # Deprecated, prog. base relative.
    R_ARM_ALU_SBREL_27_20 = 37

    R_ARM_TARGET1 = 38

    # Program base relative.
    R_ARM_SBREL31 = 39

    R_ARM_V4BX = 40

    R_ARM_TARGET2 = 41

    # 32 bit PC relative.
    R_ARM_PREL31 = 42

    # Direct 16-bit (MOVW).
    R_ARM_MOVW_ABS_NC = 43

    # Direct high 16-bit (MOVT).
    R_ARM_MOVT_ABS = 44

    # PC relative 16-bit (MOVW).
    R_ARM_MOVW_PREL_NC = 45

    # PC relative (MOVT).
    R_ARM_MOVT_PREL = 46

    # Direct 16 bit (Thumb32 MOVW).
    R_ARM_THM_MOVW_ABS_NC = 47

    # Direct high 16 bit (Thumb32 MOVT).
    R_ARM_THM_MOVT_ABS = 48

    # PC relative 16 bit (Thumb32 MOVW).
    R_ARM_THM_MOVW_PREL_NC = 49

    # PC relative high 16 bit (Thumb32 MOVT).
    R_ARM_THM_MOVT_PREL = 50

    # PC relative 20 bit (Thumb32 B<cond>.W).
    R_ARM_THM_JUMP19 = 51

    # PC relative X & 0x7E (Thumb16 CBZ, CBNZ).
    R_ARM_THM_JUMP6 = 52

    # PC relative 12 bit (Thumb32 ADR.W).
    R_ARM_THM_ALU_PREL_11_0 = 53

    # PC relative 12 bit (Thumb32 LDR{D,SB,H,SH}).
    R_ARM_THM_PC12 = 54

    # Direct 32-bit.
    R_ARM_ABS32_NOI = 55

    # PC relative 32-bit.
    R_ARM_REL32_NOI = 56

    # PC relative (ADD, SUB).
    R_ARM_ALU_PC_G0_NC = 57

    # PC relative (ADD, SUB).
    R_ARM_ALU_PC_G0 = 58

    # PC relative (ADD, SUB).
    R_ARM_ALU_PC_G1_NC = 59

    # PC relative (ADD, SUB).
    R_ARM_ALU_PC_G1 = 60

    # PC relative (ADD, SUB).
    R_ARM_ALU_PC_G2 = 61

    # PC relative (LDR,STR,LDRB,STRB).
    R_ARM_LDR_PC_G1 = 62

    # PC relative (LDR,STR,LDRB,STRB).
    R_ARM_LDR_PC_G2 = 63

    # PC relative (STR{D,H}, LDR{D,SB,H,SH}).
    R_ARM_LDRS_PC_G0 = 64

    # PC relative (STR{D,H}, LDR{D,SB,H,SH}).
    R_ARM_LDRS_PC_G1 = 65

    # PC relative (STR{D,H}, LDR{D,SB,H,SH}).
    R_ARM_LDRS_PC_G2 = 66

    # PC relative (LDC, STC).
    R_ARM_LDC_PC_G0 = 67

    # PC relative (LDC, STC).
    R_ARM_LDC_PC_G1 = 68

    # PC relative (LDC, STC).
    R_ARM_LDC_PC_G2 = 69

    # Program base relative (ADD,SUB).
    R_ARM_ALU_SB_G0_NC = 70

    # Program base relative (ADD,SUB).
    R_ARM_ALU_SB_G0 = 71

    # Program base relative (ADD,SUB).
    R_ARM_ALU_SB_G1_NC = 72

    # Program base relative (ADD,SUB).
    R_ARM_ALU_SB_G1 = 73

    # Program base relative (ADD,SUB).
    R_ARM_ALU_SB_G2 = 74

    # Program base relative (LDR, STR, LDRB, STRB).
    R_ARM_LDR_SB_G0 = 75

    # Program base relative (LDR, STR, LDRB, STRB).
    R_ARM_LDR_SB_G1 = 76

    # Program base relative (LDR, STR, LDRB, STRB).
    R_ARM_LDR_SB_G2 = 77

    # Program base relative (LDR, STR, LDRB, STRB).
    R_ARM_LDRS_SB_G0 = 78

    # Program base relative (LDR, STR, LDRB, STRB).
    R_ARM_LDRS_SB_G1 = 79

    # Program base relative (LDR, STR, LDRB, STRB).
    R_ARM_LDRS_SB_G2 = 80

    # Program base relative (LDC,STC).
    R_ARM_LDC_SB_G0 = 81

    # Program base relative (LDC,STC).
    R_ARM_LDC_SB_G1 = 82

    # Program base relative (LDC,STC).
    R_ARM_LDC_SB_G2 = 83

    # Program base relative 16 bit (MOVW).
    R_ARM_MOVW_BREL_NC = 84

    # Program base relative high 16 bit (MOVT).
    R_ARM_MOVT_BREL = 85

    # Program base relative 16 bit (MOVW).
    R_ARM_MOVW_BREL = 86

    # Program base relative 16 bit (Thumb32 MOVW).
    R_ARM_THM_MOVW_BREL_NC = 87

    # Program base relative high 16 bit (Thumb32 MOVT).
    R_ARM_THM_MOVT_BREL = 88

    # Program base relative 16 bit (Thumb32 MOVW).
    R_ARM_THM_MOVW_BREL = 89

    R_ARM_TLS_GOTDESC = 90

    R_ARM_TLS_CALL = 91

    # TLS relaxation.
    R_ARM_TLS_DESCSEQ = 92

    R_ARM_THM_TLS_CALL = 93

    R_ARM_PLT32_ABS = 94

    # GOT entry.
    R_ARM_GOT_ABS = 95

    # PC relative GOT entry.
    R_ARM_GOT_PREL = 96

    # GOT entry relative to GOT origin (LDR).
    R_ARM_GOT_BREL12 = 97

    # 12 bit, GOT entry relative to GOT origin (LDR, STR).
    R_ARM_GOTOFF12 = 98

    R_ARM_GOTRELAX = 99

    R_ARM_GNU_VTENTRY = 100

    R_ARM_GNU_VTINHERIT = 101

    # PC relative & 0xFFE (Thumb16 B).
    R_ARM_THM_PC11 = 102

    # PC relative & 0x1FE (Thumb16 B/B<cond>).
    R_ARM_THM_PC9 = 103

    # PC-rel 32 bit for global dynamic thread local data
    R_ARM_TLS_GD32 = 104

    # PC-rel 32 bit for local dynamic thread local data
    R_ARM_TLS_LDM32 = 105

    # 32 bit offset relative to TLS block
    R_ARM_TLS_LDO32 = 106

    # PC-rel 32 bit for GOT entry of static TLS block offset
    R_ARM_TLS_IE32 = 107

    # 32 bit offset relative to static TLS block
    R_ARM_TLS_LE32 = 108

    # 12 bit relative to TLS block (LDR, STR).
    R_ARM_TLS_LDO12 = 109

    # 12 bit relative to static TLS block (LDR, STR).
    R_ARM_TLS_LE12 = 110

    # 12 bit GOT entry relative to GOT origin (LDR).
    R_ARM_TLS_IE12GP = 111

    # Obsolete.
    R_ARM_ME_TOO = 128

    R_ARM_THM_TLS_DESCSEQ = 129

    R_ARM_THM_TLS_DESCSEQ16 = 129

    R_ARM_THM_TLS_DESCSEQ32 = 130

    # GOT entry relative to GOT origin, 12 bit (Thumb32 LDR).
    R_ARM_THM_GOT_BREL12 = 131

    R_ARM_IRELATIVE = 160

    R_ARM_RXPC25 = 249

    R_ARM_RSBREL32 = 250

    R_ARM_THM_RPC22 = 251

    R_ARM_RREL32 = 252

    R_ARM_RABS22 = 253

    R_ARM_RPC24 = 254

    R_ARM_RBASE = 255

    # Keep this the last entry.
    R_ARM_NUM = 256


# Program segment header.
class Phdr(Structure):
    _fields_ = [
        # Segment type
        ("p_type", Word),
        # Segment file offset
        ("p_offset", Off),
        # Segment virtual address
        ("p_vaddr", Addr),
        # Segment physical address
        ("p_paddr", Addr),
        # Segment size in file
        ("p_filesz", Word),
        # Segment size in memory
        ("p_memsz", Word),
        # Segment flags
        ("p_flags", Word),
        # Segment alignment
        ("p_align", Word),
    ]


# Legal values for p_type (segment type).
@global_enum
class PT(IntEnum):
    # Program header table entry unused
    PT_NULL = 0

    # Loadable program segment
    PT_LOAD = 1

    # Dynamic linking information
    PT_DYNAMIC = 2

    # Program interpreter
    PT_INTERP = 3

    # Auxiliary information
    PT_NOTE = 4

    # Reserved
    PT_SHLIB = 5

    # Entry for header table itself
    PT_PHDR = 6

    # Thread-local storage segment
    PT_TLS = 7

    # Number of defined types
    PT_NUM = 8

    # Start of OS-specific
    PT_LOOS = 0x60000000

    # End of OS-specific
    PT_HIOS = 0x6FFFFFFF

    # Start of processor-specific
    PT_LOPROC = 0x70000000

    # End of processor-specific
    PT_HIPROC = 0x7FFFFFFF


# Legal values for p_flags (segment flags).
@global_enum
class PF(IntEnum):
    # Segment is executable
    PF_X = 1 << 0

    # Segment is writable
    PF_W = 1 << 1

    # Segment is readable
    PF_R = 1 << 2

    # OS-specific
    PF_MASKOS = 0x0FF00000

    # Processor-specific
    PF_MASKPROC = 0xF0000000


# Dynamic section entry.
class Dyn(Structure):
    class _Un(Union):
        _fields_ = [
            # Integer value
            ("d_val", Word),
            # Address value
            ("d_ptr", Addr),
        ]

    _anonymous_ = ("d_un",)
    _fields_ = [
        # Dynamic entry type
        ("d_tag", Sword),
        ("d_un", _Un),
    ]


# Legal values for d_tag (dynamic entry type).
@global_enum
class DT(IntEnum):
    # Marks end of dynamic section
    DT_NULL = 0

    # Name of needed library
    DT_NEEDED = 1

    # Size in bytes of PLT relocs
    DT_PLTRELSZ = 2

    # Processor defined value
    DT_PLTGOT = 3

    # Address of symbol hash table
    DT_HASH = 4

    # Address of string table
    DT_STRTAB = 5

    # Address of symbol table
    DT_SYMTAB = 6

    # Address of Rela relocs
    DT_RELA = 7

    # Total size of Rela relocs
    DT_RELASZ = 8

    # Size of one Rela reloc
    DT_RELAENT = 9

    # Size of string table
    DT_STRSZ = 10

    # Size of one symbol table entry
    DT_SYMENT = 11

    # Address of init function
    DT_INIT = 12

    # Address of termination function
    DT_FINI = 13

    # Name of shared object
    DT_SONAME = 14

    # Library search path (deprecated)
    DT_RPATH = 15

    # Start symbol search here
    DT_SYMBOLIC = 16

    # Address of Rel relocs
    DT_REL = 17

    # Total size of Rel relocs
    DT_RELSZ = 18

    # Size of one Rel reloc
    DT_RELENT = 19

    # Type of reloc in PLT
    DT_PLTREL = 20

    # For debugging; unspecified
    DT_DEBUG = 21

    # Reloc might modify .text
    DT_TEXTREL = 22

    # Address of PLT relocs
    DT_JMPREL = 23

    # Process relocations of object
    DT_BIND_NOW = 24

    # Array with addresses of init fct
    DT_INIT_ARRAY = 25

    # Array with addresses of fini fct
    DT_FINI_ARRAY = 26

    # Size in bytes of DT_INIT_ARRAY
    DT_INIT_ARRAYSZ = 27

    # Size in bytes of DT_FINI_ARRAY
    DT_FINI_ARRAYSZ = 28

    # Library search path
    DT_RUNPATH = 29

    # Flags for the object being loaded
    DT_FLAGS = 30

    # Start of encoded range
    DT_ENCODING = 32

    # Array with addresses of preinit fct
    DT_PREINIT_ARRAY = 32

    # size in bytes of DT_PREINIT_ARRAY
    DT_PREINIT_ARRAYSZ = 33

    # Address of SYMTAB_SHNDX section
    DT_SYMTAB_SHNDX = 34

    # Number used
    DT_NUM = 35

    # Start of OS-specific
    DT_LOOS = 0x6000000D

    # End of OS-specific
    DT_HIOS = 0x6FFFF000

    # Start of processor-specific
    DT_LOPROC = 0x70000000

    # End of processor-specific
    DT_HIPROC = 0x7FFFFFFF


# Values of `d_un.d_val' in the DT_FLAGS entry.
@global_enum
class DF(IntEnum):
    # Object may use DF_ORIGIN
    DF_ORIGIN = 0x00000001

    # Symbol resolutions starts here
    DF_SYMBOLIC = 0x00000002

    # Object contains text relocations
    DF_TEXTREL = 0x00000004

    # No lazy binding for this object
    DF_BIND_NOW = 0x00000008

    # Module uses the static TLS model
    DF_STATIC_TLS = 0x00000010
