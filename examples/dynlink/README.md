# Dynamic Linking

One of the most useful things about MicroPython is if you want to add more code to a board you can do that simply by copying Python source files to it. However this ability works only with Python code--you can't add more C or native code to a board in this manner. To add native code, you have to go back and recompile the firmware and reflash it to the board. Wouldn't it be great if we could add native code as easily as we can add Python code, without recompiling the firmware?

This is what dynamic linking does. With an operating system (e.g., Windows or Linux) programs typically don't contain all their code in one monlithic binary like the firmware we flash on devices. Instead when they start running they load additional code from shared libraries (i.e., dll files on Windows and so files on Linux). The process through which code from shared libraries is "merged" into the executable at run-time is known as dynamic linking. And in order to get more native code onto your board while it is running, we need to do some kind of dynamic linking on the microcontroller.

## Not a shared library
Perhaps a good place to start is to compile our microcontroller code as a shared library. On Linux, this means compiling with the GCC flags `-shared` and `-fPIC`. Although it seems promising, I found this to be starting in the wrong direction. There are four reasons why:

1. The problem that shared libraries actually solve is sharing code across multiple processes at different virtual memory addresses. In an operating system environment you want to have one copy of a common library (e.g., libc) in physical memory and have it mapped into the virtual memory of multiple processes. This is not at all the problem we have on microcontrollers. We don't even have virtual memory on most microcontrollers running MicroPython.

1. Using the shared library flags introduces a lot of machinery, namely the GOT (Global Offset Table) and the PLT (Procedure Linkage Table). These tables take up space and are not necessary for solving the problem on microcontrollers. (Not surprising since shared libraries were designed to solve a different problem.)

1. A strong assumption made by the shared library model is that the distance between the code and data sections is constant (known at static link-time). This makes sense in an virtual memory environment to keep a shared library's code and data in one contiguous address block. It does not make sense for a microcontroller because code and data are loaded into different address spaces that are independently allocated. Code (being read-only) is loaded into XIP flash, data is loaded into RAM, and it's not practically possible to ensure a fixed distance between them.

1. The shared library flags are not supported in GCC for ARMv6-M, which is what an RP2040 is, which is what I'm using.

So for these reasons I'll be working with regular static executables, not shared libraries.

# Design
A technical deep dive in MicroPythonRT's dynamic linking system.

## Anatomy of an ELF file
Let's start by examining how the firmware is currently linked and flashed to the device. When building MicroPython or any application for a microcontroller, all the source files are compiled and statically linked into one executable know as the firmware. In the MicroPython RP2 port, this file is output in the build directory as firmware.elf. The file extension "elf" denotes the [ELF](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) file format for storing executables and other linkable objects. We can examine ELF files using the [`readelf`](https://man7.org/linux/man-pages/man1/readelf.1.html) command. For example, 
```
$ arm-none-eabi-readelf -S -l build-RPI_PICO/firmware.elf
```

An ELF file is divided into sections, which are listed by the above invocation of `readelf`.
```
Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .boot2            PROGBITS        10000000 001000 000100 00  AX  0   0  1
  [ 2] .text             PROGBITS        10000100 001100 06e660 00  AX  0   0 16
  [ 3] .rodata           PROGBITS        1006f000 070000 02a004 00  WA  0   0 4096
  [ 4] .binary_info      PROGBITS        10099004 09a004 000044 00   A  0   0  4
  [ 5] .ram_vector_table PROGBITS        20000000 09e124 0000c0 00   W  0   0  4
  [ 6] .data             PROGBITS        200000c0 09a0c0 004064 00  AX  0   0 16
  [11] .bss              NOBITS          20004130 09e130 005df8 00  WA  0   0 16
  [24] .symtab           SYMTAB          00000000 4b412c 03c970 10     25 12001  4
  [25] .strtab           STRTAB          00000000 4f0a9c 01eff6 00      0   0  1
  [26] .shstrtab         STRTAB          00000000 50fa92 00013a 00      0   0  1
```
Some non-interesting sections have been left out. Briefly, ".boot2" is the second stage bootloader, ".binary_info" is a [thing](https://www.raspberrypi.com/documentation/pico-sdk/runtime.html#pico_binary_info) the Pico SDK does, ".ram_vector_table" is the interrupt vector table. 

The ".text" section contatins all the compiled assembly code of the application. 

The ".rodata", ".data", and ".bss" sections contain global variables. A global variable goes into a specific section depnding on how it is defined.
```
const int x = 42; // const --> .rodata
int x = 42;       // initialized --> .data
int x;            // zero-initialized --> .bss
```

The ".symtab" section contains the symbol table, which is a table of the names of all the functions and global variables in the application along with their memory address. The ".strtab" section contains the strings used as the names in the symbol table, and the ".shstrtab" section contains the strings used as the names of the sections (e.g., ".text", ".data").

In addition to sections, the ELF file contains program headers, aka segments, which are basically instructions on how parts of the ELF should be loaded into memory. Or in the case of microcontrollers, how parts of the ELF file should be flahsed into flash.

```
Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x001000 0x10000000 0x10000000 0x6e760 0x6e760 R E 0x1000
  LOAD           0x070000 0x1006f000 0x1006f000 0x2a048 0x2a048 RW  0x1000
  LOAD           0x09a0c0 0x200000c0 0x10099048 0x04064 0x04064 R E 0x1000
  LOAD           0x000130 0x20004130 0x20004130 0x00000 0x05df8 RW  0x1000

 Section to Segment mapping:
  Segment Sections...
   00     .boot2 .text 
   01     .rodata .binary_info 
   02     .data 
   03     .bss 
```

Here the first header entry basically says, starting at offset 0x1000 in the ELF file, for a length of 0x6e760 bytes, copy this data to memory address 0x10000000. This chunk of the ELF file contains the ".boot2" and ".text" sections. On ARM systems, memory addresses beginning at 0x10000000 refer to the XIP flash memory. So it basically puts these two sections as the first things in the flash memory. After a reset, execution probably begins at the first byte in flash memory and that is now the bootloader, so that make sense.

The next header does a similar thing: it takes the ".rodata" and ".binary_info" sections and puts them in flash memory just after where the previous header left off. Since the ".rodata" section contains "const" global variables, they can be placed in flash memory since they are meant to be read-only.

The third header is a little different. It continues to copy the ".data" section to the next available flash memory address, but its VirtAddr != PhysAddr. On ARM systems, memory addresses beginning at 0x20000000 refer to RAM memory. What this header says is: take the ".data" section from the ELF file and flash it to 0x10099048 in flash memory, but also after reset, copy it from that flash location to 0x200000c0 in RAM memory. The ".data" section contains initialized but mutable values, so their initial values have to be stored in flash memory while the device is off. However when the device turns on, the initial values are copied into RAM so that they can be mutated by the program.

The fourth and final header is also different because it has a FileSiz of zero. The ".bss" section contains uninitialized global variables, which are in fact initialized with zeroes in C. Since the contents of this section is all zeroes, it does not have to be stored in the ELF file. This header is saying: at RAM address 0x20004130, reserve 0x5df8 bytes for the zero-initialized ".bss" section. After reset, some code will zero-out this chunk of memory.

When this ELF file is flashed to a device (e.g., via UF2) it will create the following layout in flash and RAM.
```
                  Flash                                     RAM
           |-------------------|                   |-------------------| 
0x10000000 | .boot2            |        0x20000000 | .ram_vector_table |
           | .text             |        0x200000c0 | .data             |
           |                   |                   |                   |
           |                   |                   |                   |
           |                   |                   |-------------------|
           |-------------------|        0x20004130 | .bss              | 
0x1006f000 | .rodata           |                   |                   |
           |                   |                   |                   |
           |                   |                   |-------------------|
           | .binary_info      |        0x20009f28 |                   |
           |-------------------|
0x10099048 | .data             |
           |                   |
           |                   |
           |-------------------|
0x1009d0ac |                   |
```

The remaining space at the end of the flash memory can be used for a flash file system. The remaining space at the end of RAM memory is used for the heap and stack.

## Dynamic address space layout
So far we've seen what the state of the device is after we've flashed the initial firmware. Now we want to flash more code to the device without disturbing what is already there. What would that look like? There is no standard way to do this but I propose it look like this:
```
                  Flash                                     RAM
           |-------------------|                   |-------------------| 
0x10000000 | FIRMWARE          |        0x20000000 | FIRMWARE          |
           | .boot2            |                   | .ram_vector_table |
           | .text             |                   | .data             |
           | .rodata           |                   | .bss              |
           | .binary_info      |                   |                   |
           | .data             |                   |                   | 
           |                   |                   |                   |
           |                   |                   |-------------------|
           |                   |        0x20009f28 | EXTENSION         |
           |                   |                   | .data             |
           |-------------------|                   | .bss              |
0x1009d0ac | EXTENSION         |                   |                   |
           | .text             |                   |                   |
           | .rodata           |                   |-------------------|
           | .data             |                   |                   |
           |                   |
           |                   |
           |                   |
           |-------------------|
           |                   |
```

All of the sections of the firmware stay as is. All of the sections for the extension module are written sequentially after the firmware at the next available address. Some sections (like ".boot2") are only meaningful for the firmware, so the extension module contains only standard sections for code and data.

The ability to do this relies on there being free space after the end of the firmware. For RAM this is no problem since that space is used for the heap. Since the heap is dynamically allocated, it just means the program starts with less available heap space. For flash, typically there is a fixed size block of flash allocated for the file system. This block is typically placed at the end of the address space. This means there is free flash space available for loading extensions between the end of the firmware and the beginning of the file system.

## Dynamic relocations
We have a plan on how to layout in memory the sections of our extension's ELF file. We now turn to a much harder problem: the contents of those sections themselves.

In the case of linking the firmware's ELF file, the linker knows the memory address of where every function and global variable will be loaded. It knows this because it starts packing these things from the fixed address 0x10000000 (or 0x20000000 for RAM objects) and from there knows exactly where it puts things. 

This is not the case for the extension's ELF file. In the example above, the extension's sections where loaded starting from address 0x1009d0ac, but this is just an example. On this particular device, the firmware happened to end at this address, but on another similar device with different firmware, the firmware may end at a different address. Moreover, if we were to load multiple extension modules sequentially, the load address of the current extension module depends on what other modules where loaded before it.

Another problem is that the extension module needs to be able to call functions and access data that are defined in the firmware. When the extension module is built, the linker doesn't know the exact version of the firmware it will be loaded against and so doesn't know the addresses of those objects.

Fortunately there is already a well-known solution to these problems that is already used by the linker. When a C file is compiled it produces an O file, which is also a kind of ELF file. In the O file, the code doesn't know at what address it is going to end up at, or the addresses of objects outside itself. The linker is able to take all these O files containing partial information and produce an executable where all the addresses are determined and everything is "linked" together. It does this using another part of an ELF file called the relocation table.

An O file contains a relocation table stored as a section within the ELF file. The table contains the location within the O file of every reference to a function or global variable whose address is not yet known. Normally executable ELF files (like the firmware) don't include the relocation table, but we can force the linker to keep it with the `-q` options and then examine the table using `readelf`.
```
Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 2] .text             PROGBITS        10000100 001100 039c34 00  AX  0   0 16
  [ 3] .rel.text         REL             00000000 3d5e04 0131c0 08   I 42   2  4
  [ 4] .rodata           PROGBITS        10039d38 03ad38 00e5a0 00  WA  0   0  8
  [ 5] .rel.rodata       REL             00000000 3e8fc4 008488 08   I 42   4  4
  [12] .data             PROGBITS        200000c0 04a0c0 004634 00  AX  0   0 16
  [13] .rel.data         REL             00000000 3f14dc 000de0 08   I 42  12  4
  [19] .bss              NOBITS          20004718 04f718 00384c 00  WA  0   0  8
```
Looking at the section headers again we now see the additional sections for relocation tables. Each of the sections ".text", ".rodata", and ".data" now have an associated ".rel.*" section which is its relocation table. We can also examine the relocation tables themselves using `readelf -r`.
```
Relocation section '.rel.text' at offset 0x3d5e04 contains 9784 entries:
 Offset     Info    Type            Sym.Value  Sym. Name
10006514  001d0702 R_ARM_ABS32       1003c898   mp_type_int
10006528  0024a70a R_ARM_THM_CALL    10014775   mp_raise_TypeError
10006532  0024ec0a R_ARM_THM_CALL    1000cf25   mp_obj_list_make_new
1000653e  0022270a R_ARM_THM_CALL    1000cd45   mp_obj_list_sort
10006548  00000302 R_ARM_ABS32       10039d38   .rodata
1000654c  001b5b02 R_ARM_ABS32       1003c964   mp_type_list
10006556  00226b0a R_ARM_THM_CALL    1000f3cf   mp_obj_str_get_qstr
```
This is only an excerpt from one relocation table, as the tables are very large. The first line says, "at address 0x10006514 according to this ELF file, there is a reference to the global variable `mp_type_int`. I don't know what the final address of this variable will be so you have to fix it up later according to the pattern `R_ARM_ABS32`." The current value of the symbol (i.e., the address of the global variable) is 0x1003c898, but that might be different "later" when the fixup occurs.

Information about the symbols is stored in the symbol table section. You can view this table using `readelf -s`.
```
Symbol table '.symtab' contains 9486 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
  7577: 10013b45    24 FUNC    GLOBAL DEFAULT    2 mp_load_attr
  7578: 1000c573   110 FUNC    GLOBAL DEFAULT    2 mp_obj_int_unary_op
  7579: 10012d5b    28 FUNC    GLOBAL DEFAULT    2 qstr_data
  7580: 100089c9   216 FUNC    GLOBAL DEFAULT    2 mpz_xor_inpl
  7581: 20000ab7    68 FUNC    GLOBAL DEFAULT   12 spi_write16_blocking
  7582: 1002f0d1     0 FUNC    GLOBAL DEFAULT    2 uint642double_shim
  7583: 10014eb1    12 FUNC    GLOBAL DEFAULT    2 mp_stack_ctrl_init
```
The table contains the name of all the symbols defined or used in the ELF, what their current value/address is, and other information.

Using these relocation tables, which are already generated by the linker, there is enough information for the microcontroller to "relocate" extension modules as they are flashed onto the device at a dynamically determined load address.

## Dynamic discovery
Once we have extensions modules flashed onto the device, one step remains: how does the program discover and access them? First as we appended extension modules to the end of memory, we need to keep track of some metadata about how big each module is and when is the last one. Second we need to be able to locate important bits inside each module, such as its symbol table so we can look up symbols, or its array of init functions which initialize the module at run-time.

In the world of shared libraries, the second problem is solved by the "dynamic" section. This is a special section in the ELF file that tells the run-time system about these important bits. Although we are not creating shared libraries, or even doing true dynamic linking, we will reuse the format and structure of the dynamic section since it basically covers everything we'd need, even though we are using it in a different context that what it was intended.

# Implementation
Summarizing the previous section, our rough plan to implement dynamic linking for microcontrollers is:
1. Load extensions modules into the memory space between the firmware and heap/file system.
1. Use the relocation tables generated by the standard linker to relocate extension modules to dynamic load addresses.
1. Write code for the microcontroller that will perform these relocations when flashing an extension module onto the device.
1. Provide an API for microcontroller programs to access the extension modules that have been added to the device.



## Changes to firmware
In order to support the dynamic loading of extension modules we need to make some changes to the underlying firmware. It is not possible to flash extension modules unless support for that is enabled in the firmware.

### The flash heap
We need a memory manager that will manage the space where the extension modules are flashed. This abstraction is named the [flash heap](/ports/rp2/newlib/flash_heap.c), because naming things is hard. Starting at the end of the firmware, the flash heap sequentially allocates chunks of flash and RAM in this space. The heap can keep allocating space until the flash space runs into the start of the file system, or the RAM space runs in to the end of RAM.
```
                  Flash                                     RAM
           |-------------------|                   |-------------------| 
0x10000000 | FIRMWARE          |        0x20000000 | FIRMWARE          |
           |                   |                   |                   |
           |                   |                   |                   |
           |                   |                   |                   |
           |-------------------|                   |-------------------|
           |                   |                   |                   |
           |      space        |                   |      space        |
           |    managed by     |                   |    managed by     |
           |    flash heap     |                   |    flash heap     |
           |                   |                   |                   |      
           |-------------------|                   |-------------------|
           | FILE SYSTEM       |                   | RUNTIME HEAP      |
           |                   |                   | (malloc)          |
           |                   |                   |                   |
           |                   |                   |                   |
           |-------------------|                   |-------------------|
               end of flash                              end of RAM
```
Allocating RAM from the flash heap takes away RAM available for use as the runtime heap (i.e., by `malloc`), so in practice you'd never want to come close to exhausting this space. When allocating RAM this way, it cannot be immediately available because it is being used by the current runtime heap. Therefore after flashing a new extension module the device must be reset. During restart, the requested RAM is reserved and the new runtime heap starts after this reservation.

The flash heap can only allocate sequentially and cannot deallocate. The reason is because this space is used to store pointers into itself, parts of it cannot be safely deallocated without potentially creating dangling points. For example, imagine loading two extension modules A and B, where B depends on A. A cannot be deallocated because it is pointed to from B. Instead of deallocation, the flash heap can be truncated to an earlier state, but in practice this state often ends up being the initial state after the end of the firmware. For example, if you flash a bunch of extension modules, and then decide you don't want some of them, you pretty much have to delete them all back to the firmware and then reflash the ones you want. This is considered okay because flashing code to a device should not be a frequent operation and it greatly simplies the design of the flah heap.

As a bonus, this same flash heap is also used to support the Python [module freezing](/examples/freeze/README.md) functionality of MicroPythonRT.

### Symbol tables
To support dynamic loading we need to have a copy of the firmware's symbol table on the device in order to resolve symbol requests from extension modules. Although the symbol already exists in the firmware's ELF file, it does not get flashed onto the device. To fix this we will post-process `firmware.elf` to have the symbol table flashed onto the device into the flash heap. Here's a look at the post-processed ELF file.
```
Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [13] .flash_heap       PROGBITS        1004d000 052000 000010 00  WA  0   0 4096
  [14] .dynamic          DYNAMIC         1004d010 052010 000040 08   A 16   0  4
  [15] .hash             HASH            1004d050 052050 000008 00   A 17   0  4
  [16] .dynstr           STRTAB          1004d058 052058 00aaa6 00   A  0   0  1
  [17] .dynsym           DYNSYM          10057b00 05cb00 009c90 10   A 16   1  4
  [18] .flash_heap_end   PROGBITS        10061790 066790 000010 00   A  0   0  4
```
These sections were added to the file (none were removed).
- ".flash_heap" is the beginning of the flash heap and contains metadata for the flash heap. For simplicity, the firmware's dynamic linking information is inserted into the flash heap as though it were loaded like an extension module.This way when the run-time system examines the flash heap, it sees the firmware as the first module and treats it uniformly like other modules.
- ".dynamic" is the dynamic section for describing the firmware. See details below.
- ".hash", ".dynstr", and ".dynsym" for the dynamic symbol table for the firmware. The original symbol table sections (".strtab" and ".symtab") still exist unchanged in the ELF file. The original table was filtered and copied to produce the dynamic table. The original symbol contains more symbols than we need at run-time so it is pared down to only include the "public" symbols that can be used for dynamic linking.
- Finally ".flash_heap_end" is the current end of the flash heap and contains the metadata to mark its termination.

In this example there were 2505 entries in the symbol table, and the table along with its strings takes up 81 kB of flash space on device. Some things can be done to reduce the size of the symbol table. The best thing is to use the `static` keyword in C as much as is appropriate. When a function or global variable is marked `static` it is not visible outside of its file, so cannot be involved in dynamic linking, and is not included in the symbol table.

Finally here is a sneak peek of the dynamic section which we'll revist later. It primarily contains the addresses of the symbol table parts.
```
Dynamic section at offset 0x52010 contains 8 entries:
  Tag        Type                         Name/Value
 0x00000004 (HASH)                       0x1004d050
 0x00000005 (STRTAB)                     0x1004d058
 0x00000006 (SYMTAB)                     0x10057b00
 0x0000000a (STRSZ)                      43686 (bytes)
 0x0000000b (SYMENT)                     16 (bytes)
 0x0000000e (SONAME)                     Library soname: [firmware.elf]
 0x0000001e (FLAGS)                      TEXTREL BIND_NOW
 0x00000000 (NULL)                       0x0
```

### Linker options
Some libraries, particularly SDKs, contain a lot of functions. If your program uses part of the library, but not all of it, it is possible to exclude the parts you don't use from your executable by using the `--gc-sections` linker option. (Indeed, this option is on by default in the Pico SDK.)

Obviously the firmware needs to use the platform SDK, or at least parts of it. Probably extension modules will also need to use the platform SDK too. However the extension modules cannot simply bring their own copy of the platform SDK with them because then there would be two copies of the same data and functions at run-time. So extension modules need to have an expectation of what libraries will be dynamically provided by the firmware.

Saying the firwmare will provide the platform SDK to extensions seems like a reasonable restriction. However if the extension uses a particular SDK function that is not already used by the firmware, and the firmware is linked with `--gc-sections`, then that function will be missing at run-time and dynamic linking of the extension will fail.

So unfortunately we cannot use the `--gc-sections` option when building for dynamic linking. This has the possibility of drastically increasing the size of the firmware. In a experiment using the RP2 port of MicroPython, the code and data size increased from 275 kB to 305 kB by disabling `--gc-sections`, so 30 kB. No one said support dynamic linking would be cheap. It is a large and powerful feature and has a commensurate cost.

A similar situation exists with the common C library, libc_nano. It make sense that the firmware should make this available to extension modules, but extension modules may want to use of the library than what the firmware happens to use. So a subset of libc_nano is guaranteed to be linked into the firmware for extension modules to use.

## Building extension modules
Extension modules are built as their own executables separate from the firmware executable. During linking extension modules use the `-q` option to preserve relocation info in the output ELF. After linking, the ELF file is post-processed to:
- create dynamic symbol info (similar to the firmware post-processing case),
- massage the relocation data,
- special processing for MicroPython.

### Dependencies
The extension module is going to depend on functions that are part of the firmware executable. Without telling the linker about this dependency on the firmware, blindly linking the extension module will produce a lot of "undefined reference to blah" errors. In the world of shared libraries, this problem would be handled simply by specifying the firmware shared library as an input when linking the extension shared library. In `-shared` mode the linker knows to use input shared libraries just for checking symbols and does not actually include them in the output.

But we aren't using `-shared` mode. If we tried the same thing in static mode, then although the undefined symbols would be satisfied, the firmware static library would be statically linked into the extension executable, which is not what we want. Instead the solution lies with an uncommon linker option. The `-R` option to the linker pulls in symbols from another ELF file but doesn't include that ELF file into its output.
```
$ arm-none-eabi-ld -Rfirmware.elf extension.o ...
```
In a linker invocation like this, if there is an undefined reference from any of the O files of the extension, the linker will consider this reference satisfied if it appears in the symbol table of firmware.elf, however it does not then include this function in the output. The reference to this symbol in the output is bogus, but this is okay because it will be relocated by the dynamic linker later. On the other hand, if the symbol is not defined in firmware.elf, you still get the "undefined reference" error as expected.

Another advantage of this approach is the symbol table information from the `-R` option communicates to the linker whether the referenced function is in flash or RAM and encourages the correct code generation of Thumb veneers.

### Post-processing
After linking the extension we post-process the output ELF file to include the extra information needed to support dynamic linking.
```
Section Headers:
  [Nr] Name              Type            Addr     Off    Size   ES Flg Lk Inf Al
  [ 0]                   NULL            00000000 000000 000000 00      0   0  0
  [ 1] .text             PROGBITS        10100000 0000b8 0034f8 00  AX  0   0  8
  [ 2] .rodata           PROGBITS        101034f8 0035b0 0038d8 00   A  0   0  4
  [ 3] .data             PROGBITS        20000000 006e88 0048b0 00  AX  0   0  8
  [ 5] .bss              NOBITS          200048b0 00b738 000000 00  WA  0   0  1
  [ 8] .dynamic          DYNAMIC         1010b680 00b7a8 000068 08   A 10   0  4
  [ 9] .hash             HASH            1010b6e8 00b810 000008 00   A 11   0  4
  [10] .dynstr           STRTAB          1010b6f0 00b818 0006b6 00   A  0   0  1
  [11] .dynsym           DYNSYM          1010bda8 00bed0 000680 10   A 10  12  4
  [12] .rela.dyn         RELA            1010c428 00c550 001818 0c     11   0  4
  [13] .shstrtab         STRTAB          00000000 00dd68 00007a 00      0   0  1
  ```
Looking at the section header table, the added sections ".dynamic", ".hash", ".dynstr", and ".dynsym" should be familiar from the post-processing that was done on the firmware ELF file. Just like in the firmware case, the original symbol table is filtered and copied to produce the dynamic symbol table.

A new added section is ".rela.dyn" which contains the relocation table. The original relocation tables output by the linker via the `-q` option were filtered and copied to produce the dynamic relocation table. We'll look at this relocation table in more detail when we get to dynamic linking.
```
Program Headers:
  Type           Offset   VirtAddr   PhysAddr   FileSiz MemSiz  Flg Align
  LOAD           0x0000b8 0x10100000 0x10100000 0x06dd0 0x06dd0 R E 0x1000
  LOAD           0x006e88 0x20000000 0x10106dd0 0x048b0 0x048b0 R E 0x1000
  LOAD           0x00b7a8 0x1010b680 0x1010b680 0x00da8 0x00da8 R   0x4
  DYNAMIC        0x00b7a8 0x1010b680 0x1010b680 0x00068 0x00068 R   0x4

 Section to Segment mapping:
  Segment Sections...
   00     .text .rodata 
   01     .data 
   02     .dynamic .hash .dynstr .dynsym 
   03     .dynamic 
```
Here in the program headers we see that the dynamic symbol table sections are loaded into flash. There is a special program header type to indicate the dynamic section. Also notice that the ".rela.dyn" section does not get loaded into flash. This section is only needed during the flashing process itself and is not used afterwards, so it doesn't need to be stored anywhere.

```
Dynamic section at offset 0xb7a8 contains 13 entries:
  Tag        Type                         Name/Value
 0x00000004 (HASH)                       0x1010b6e8
 0x00000005 (STRTAB)                     0x1010b6f0
 0x00000006 (SYMTAB)                     0x1010bda8
 0x00000007 (RELA)                       0x1010c428
 0x00000009 (RELAENT)                    12 (bytes)
 0x00000008 (RELASZ)                     6168 (bytes)
 0x0000000a (STRSZ)                      1718 (bytes)
 0x0000000b (SYMENT)                     16 (bytes)
 0x0000000e (SONAME)                     Library soname: [libaudio_mp3.elf]
 0x0000001e (FLAGS)                      TEXTREL BIND_NOW
 0x0000000c (INIT)                       0x10100005
 0x0000000d (FINI)                       0x1010004d
 0x00000000 (NULL)                       0x0
```
Finally here is the contents of the dynamic section. HASH, STRTAB, SYMTAB, RELA give the addresses of the ".hash", ".dynstr", ".dynsym", ".rela.dyn" sections respectively. Following that there is some info about the sizes of these sections. SONAME is the name of the extension module. INIT and FINI are the addresses of functions to initialize and deinitialize the extension module.

### Usage
CMake functions are available to help create an extension modules.
```
add_executable(firmware)
add_import_library(firmware_import firmware)

add_dynamic_library(extension)
target_link_libraries(extension firmware_import)
target_sources(extension
    extension.c
)
```
We assume that the firmware is built by a CMake target called `firmware`. The function `add_import_library` creates an "import" library to allow extension modules to declare firmware as a dependency. Internally it just creates an empty interface library to propagate the `-R` linker option.

The function `add_dynamic_library` creates an extension module. It addition to creating a CMake executable target for the module, it also creates a command to post-process the ELF file. Then dependency on the firmware is declared via the `firmware_import` target, not the `firmware` target itself. Adding a link library dependency on an actual library target will statically link that library into the extension.

Once the extension module is built it will output an ELF file. This ELF file is then converted to UF2 for flashing onto the device. Note that extension modules use a different UF2 loader than the boot UF2 loader used to flash the firmware. This extension UF2 loader can be started from within MicroPython.

## Loading extension modules
At run-time the system will be presented with an ELF/UF2 file, built to the specifications described above, and be asked to flash it onto the device. This process involves three steps:
1. Copying data from the ELF/UF2 file into flash memory,
1. Performing the relocations prescribed in the ELF file,
1. Making the new module known to the rest of the system.

### Copying data
The UF2 file tells the device to flash blocks of data at the addresses defined in the ELF file, however these are not the actual addresses that will be used dynamically. First the UF2 loader asks the flash heap to allocate space for the incoming data. Consequently it gets the base addresses of the flash and RAM segments that the data copy will start at. These are important numbers because everything in the ELF file needs to be relocated from its nominal address in the ELF file to a new address in the system relative to these base addresses. The loading process automatically translates the static address from the ELF file to the dynamic address assigned by the flash heap.

In order the simplify writing to flash, the flash heap maintains an in-RAM block cache of partially written flash blocks, and exposes a file-like API to the system for writing data. The file-like API allows the system to randomly seek anywhere in the flash heap to read or write data. The data lives in a RAM until the file is closed and all blocks are written to flash, or memory pressure cause some blocks to be evicted from the cache and written to flash.

### Relocating
Relocation is the crux of the whole dynamic linking system. After copying the extension module's contents into the flash heap, it then processes the relocation table.

Although there are a bewildering number of relocation types, in practice 99% of them are either `R_ARM_ABS32` or `R_ARM_THM_CALL`. (Note that relocation types are specific to a CPU architecture and we are only covering ARM/Thumb architecture here.) Crudely, `R_ARM_ABS32` fixes up access to a global variable and `R_ARM_THM_CALL` fixes up a call to a function. As an example we'll walk through the relocation to a global variable. Within this there are two cases: when the global variable is defined in the module being loaded, and when it is not. In the latter case it will typically defined in the firmware.

For a very simple example consider this C code.
```
int x;
const int *px = &x
```
We have an uninitialized global variables `x`, and an initialized global variable `px` that is initialized to the address of `x`. The variable `x` being uninitialized will live in the ".bss" section and consequently there is no storage for it in the ELF file. On the other hand, `px` being initialized and const will live in the ".rodata" section, its initial value will be stored in the ELF, and it will be copied into flash memory when the module is loaded.

When this code is built as an extension module, we get the following relocation and symbol entries.
```
Relocation section
 Offset     Info    Type            Sym.Value  Sym. Name
10100020  00002802 R_ARM_ABS32       10100024   x

Symbol table
Num:    Value  Size Type    Bind   Vis      Ndx Name
 40: 10100024     4 OBJECT  GLOBAL DEFAULT    2 x
```
From the relocation entry we see that the linker decided to put the variable `px` at address 0x10100020, which will be somewhere in the ".rodata" section. Furthermore the relocation entry tells us that the value at this address should be the address of the variable `x`, and that the address os `x` is 0x10100024 before relocation.

The base address used by this ELF file is 0x10100000. When we go to load this module, the base address we got from the flash heap is 0x10007000. So relocation means remapping the address 0x10100000 to 0x10007000. Therefore an address in the ELF file is converted to an address in the system by subracting 0xf9000 (i.e., 0x10100000 - 0xf9000 == 0x10007000).
```
         ELF        system
Symbol   address    address
x        10100024   10007024
px       10100020   10007020
```

So to perform the relocation, the system writes the value 0x10007024 (the relocated address of `x`) to the address 0x10007020 (the relocated address of `px`). Initially after copying data, the value at address 0x10007020 would have been 
0x10100024, which is the value stored in the ELF file, before relocation. The relocation is needed to updated this value to the new address after relocation.

If however the variable `x` was `extern` and defined in the firmware, the process would be slightly different.
```
extern int x;
const int *px = &x
```
We would instead end up with table entries that look like this.
```
Relocation section
 Offset     Info    Type            Sym.Value  Sym. Name
10100020  00007702 R_ARM_ABS32       10000060   x

Symbol table
Num:    Value  Size Type    Bind   Vis      Ndx Name
119: 10000060     4 OBJECT  GLOBAL DEFAULT  UND x
```
Here the "UND" cell in the symbol table tells us that the symbol `x` is undefined and that the dynamic linking needs to search for it. Although the symbol does have a value (0x10000060), this value comes from the `-R` linker option we used to create this ELF file and is pretty much meaningless.

The dynamic linker finds undefined symbols by scanning through the firmware and extension modules that have already been loaded. So starting with the firmware, the dynamic linker looks up the symbol "x" in the firmware's symbol table. If it is not found, then the dynamic link fails and the module cannot be loaded. If it is found, then the symbol's found value is used to perform the relocation.

For example, say when we looked up "x" it was found to have the value 0x10003000. This means that the actual address of `x` as defined in the firmware is 0x10003000. So then this value would be written to the address 0x10007020 (the relocated address of `px`) to perform the relocation.

### Application Interface
The system provides an API based on [dlsym](https://man7.org/linux/man-pages/man3/dlsym.3.html) to allow programs to gain access to functions inside an extension module.
```
void *dlsym(void *handle, const char *symbol);
```

The caller passes the name of a function and if that function is found in one of the loaded extension modules, then `dlsym` will return a pointer to it.

Other functions in this API (e.g., `dlopen`, `dlclose`) are effectively no-ops because of operating differences between a full OS and a microcontroller. In an OS, `dlopen` loads a shared library from disk into RAM, this happens everytime the program is run (because writing to RAM is cheap), and there's no concept of flash. In a microcontroller, there's not enough RAM to load a library into, and writing to flash is expensive and shouldn't happen every time the program is run. 

In the dynamic loading model for microcontrollers, `dlopen` is more like the one-off step that loads the library into flash. Once a library is put into flash it is logically never unloaded or reloaded, even when the device resets. Accordingly any initializers in the extension modules are run when the device resets, at the same time as initializers from the firmware are run. They are not run on first use of the extension module.

# MicroPython
If we were only interested in dynamically linking C modules, then we could stop here. However modules that present a MicroPython module introduce an additional level of complexity.

MicroPython has a concept known as qstrs. A qstr is basically an integer used to memoize a string value in order to make string comparisons and lookups faster. The mapping from these integers to strings is globally unique and determined by run-time behavior.

When the firmware executable is built, MicroPython has a preprocessing step that collects all the qstrs defined in the code and creates a table for them, assigning them the qstr IDs 1 through to however many qstrs there are. At run-time as Python code is compiled, it dynamically adds its qstrs to the table starting at the last ID where the firmware left off.

Unfortunately things a much for complicated when building an extension module. Although we can similarly collect all the qstrs defined in the extension module's code, we do not know:
1. What was the last qstr ID used by the firmware.
1. What actual qstrs are defined in the firmware. Since qstrs are globally unique, if the extension module happens to use a string that was already used in the firmware, then we have to reuse the same qstr ID used in the firmware.
1. What qstrs were added at run-time through compilation of arbitrary Python code.
1. What qstrs were added by other previously loaded extension modules.

## Indirection of qstrs
Nevertheless, the first set in solving this problem is to collect all the qstrs used by the extension module and put them in a table numbered 1 to n. When the module is loaded, all the module's strings are added to the global qstrs table, where they get assigned a qstr ID, which is then stored in the module's local qstr translation table. This is pretty much the same approach used by mpy files, and with some preprocessor magic is even source-compatible with existing code.

```
qstr qstr_table[8];

#define MP_QSTR_foo (qstr_table[5])
```
In this example, there are 8 qstrs defined by the extension module. The string "foo" is assigned index 5. The QSTR macro is defined, instead of being the literal 5, to lookup the 5-th entry in the qstr table. At load time, the system will initialize the qstr table, and put in the 5-th entry whatever qstr ID the system is using for the string "foo". 

So far so good. However a major use of qstrs in C code is defining a static dictionary of members for a type or module. For example,
```
STATIC const mp_rom_map_elem_t my_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__),        MP_ROM_QSTR(MP_QSTR_my_module) },
    ...
};
```

To be a static initializer for a global variable, an expression must evaluate to a compile-time constant. Sadly the expression `qstr_table[5]` does not, meaning that this code no longer compiles. One option would be to abandon the use of static initializers and instead initial the global variable in a initialization function. For example,
```
void my_module_init(void) {
  my_module_globals_table[0].value = MP_QSTR_my_module;
  ...
}
```
But this has a host of problems, the most important of which is not being source-compatible with existing code. So to get this to work we basically need to "relocate" qstrs, similar to how other symbols are relocated during the dynmaic linking process.

## Relocating qstrs
When the extension module is compiled, qstrs that appear in static initializer are wrapped with `MP_ROM_QSTR(...)`, which causes their local qstrs index to be emitted instead of their qstr table lookup. That is,
```
MP_QSTR_foo              ==> (qstr_table[5])
MP_ROM_QSTR(MP_QSTR_foo) ==> 5
```
Being just an integer, the static initializer can compile, but the global variable is not correctly initialzed and we have to fix it up during dynamic linking.

Additionally at compile-time, all static MicroPython objects that contain qstrs are marked so that they can be discovered by the dynamic linker. In practice so far, the only such objects appear to be modules and types. These are core MicroPython structs, so the linker knows where the qstrs are in the struct and can replace the local index emitted by the compilier with the global qstr ID used at run-time.

# Next steps
Check out the [guide](/getstarted.md) for getting started using dynamic linking in MicroPythonRT.

Happy dynamic linking!
