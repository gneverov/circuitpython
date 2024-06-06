# SPDX-FileCopyrightText: 2024 Gregory Neverov
# SPDX-License-Identifier: MIT

import itertools
import makeqstrdata
import re
import sys


qstr_re = re.compile(r"MP_QSTR_([A-Za-z0-9_]*)")
module_re = re.compile(r"\s*MP_REGISTER_MODULE\(MP_QSTR_(.*?),\s*(.*?)\);")

static_qstrs = [makeqstrdata.qstr_escape(q) for q in makeqstrdata.static_qstr_list]


def extract_qstrs(source_files):
    qstrs = set()
    modules = []
    for src in source_files:
        with open(src) as file:
            for line in file:
                for m in qstr_re.finditer(line):
                    qstrs.add(m.group(1))
                for m in module_re.finditer(line):
                    modules.append((m.group(1), m.group(2)))
    qstrs.difference_update(static_qstrs)
    return sorted(qstrs), modules


def gen_header(module_name, qstrs, file=None):
    print("// This file was automatically generated by makeqstrdata.py", file=file)
    print("#pragma once", file=file)
    print('#include "py/qstr.h"', file=file)
    print(file=file)

    print("typedef const void *mp_rom_obj_t;", file=file)
    print("#define MP_ROM_INT(i) MP_OBJ_NEW_SMALL_INT(i)", file=file)
    print("#define MP_ROM_QSTR(q) MP_OBJ_NEW_QSTR(q ## _ROM)", file=file)
    print("#define MP_ROM_PTR(p) (p)", file=file)
    print("#define MP_ROM_QSTR_CONST(q) (q ## _ROM)", file=file)
    print("#define MP_REGISTER_MODULE(module_name, obj_module)", file=file)
    print(
        '#define MP_REGISTER_OBJECT(obj) __attribute__((section(".init_object"), visibility("hidden"))) const mp_rom_obj_t __init_ ## obj = MP_ROM_PTR(&obj);',
        file=file,
    )
    print(file=file)

    print("enum {", file=file)
    for i, q in enumerate(itertools.chain(static_qstrs, qstrs)):
        print(f"    MP_QSTR_{q}_ROM = {i+1}, ", file=file)
    print("};", file=file)
    print(file=file)

    for q in static_qstrs:
        print(f"#define MP_QSTR_{q} MP_QSTR_{q}_ROM", file=file)
    for i, q in enumerate(qstrs):
        print(f"#define MP_QSTR_{q} (mp_extmod_qstr_table[{i}])", file=file)
    print(file=file)

    print(f"#define MP_EXTMOD_NUM_QSTRS {len(qstrs)}", file=file)
    print("extern const uint16_t mp_extmod_qstr_table[];", file=file)
    print(file=file)


def gen_source(module_name, qstrs, modules, file=None):
    print("// This file was automatically generated by makeqstrdata.py", file=file)
    print(f'#include "genhdr/{module_name}.qstrdefs.h"', file=file)
    print('#include "extmod/freeze/extmod.h"', file=file)
    print(file=file)

    print('__attribute__((visibility("hidden")))', file=file)
    print(f"const uint16_t mp_extmod_qstr_table[{len(qstrs)}];", file=file)
    print(file=file)

    print("extern const mp_rom_obj_t __init_object_start;", file=file)
    print("extern const mp_rom_obj_t __init_object_end;", file=file)
    print(file=file)

    print('__attribute__((used, visibility("default")))', file=file)
    print("const mp_extension_module_t mp_extmod_module = {", file=file)
    print(f"    .num_qstrs = {len(qstrs)},", file=file)
    print("    .qstr_table = mp_extmod_qstr_table,", file=file)
    print("    .qstrs = (const char *const []) {", file=file)
    for q in qstrs:
        print(f'        "{q}",', file=file)
    print("    },", file=file)
    print("    .object_start = &__init_object_start,", file=file)
    print("    .object_end = &__init_object_end,", file=file)
    print("};", file=file)
    print("", file=file)

    if modules:
        # The module with the shortest name is the "main" module.
        _, m = min(modules, key=lambda x: x[0])
        print(f"extern const mp_obj_module_t {m};", file=file)
        print(file=file)

        print('__attribute__((used, weak, visibility("default")))', file=file)
        print("mp_obj_t mp_extmod_init(void) {", file=file)
        print(f"    return MP_OBJ_FROM_PTR(&{m});", file=file)
        print("}", file=file)
        print(file=file)


if __name__ == "__main__":
    module_name = sys.argv[1]
    qstrs, modules = extract_qstrs(sys.argv[2:])
    with open(f"genhdr/{module_name}.qstrdefs.h", "w") as file:
        gen_header(module_name, qstrs, file)
    with open(f"genhdr/{module_name}.qstrdefs.c", "w") as file:
        gen_source(module_name, qstrs, modules, file)
