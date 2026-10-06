#!/usr/bin/env python3
"""Writes the linker script: the code of src/ and PS2SDK's libraries, then the data, each section starting with the retail
executable's (src/data/, the PS2 side's VU microcode and its programs' sizes) in the retail order, the rest of src/'s after it.

Each section follows the one before it at the alignment the retail ELF has it at (.vutext 16, the rest 64). The retail data's
objects keep their order and the spaces between them (include/retaildata.h), so the game's code finds them where it did in the
retail executable, relative to each other.

    tools/make_ld.py <output> [src objects...]
"""
import sys
from pathlib import Path

# (output section, alignment, the retail data's objects and their input sections, src/'s input sections)
SECTIONS = [
    (".vutext", 16, [("build/src/platform/ps2/renderer/vumicrocode.cpp.o", ".vutext")], None),
    (".data", 64, [("build/src/data/data.cpp.o", ".data"), ("build/src/platform/ps2/renderer/vuprogramsizes.cpp.o", ".data")],
     ".data .data.*"),
    (".rodata", 64, [("build/src/data/rodata.cpp.o", ".rodata")], ".rodata .rodata.*"),
    (".sdata", 64, [("build/src/data/sdata.cpp.o", ".sdata")], ".sdata .sdata.*"),
    (".sbss", 64, [("build/src/data/sbss.cpp.o", ".sbss")], ".sbss .sbss.* .scommon"),
    (".bss", 64, [("build/src/data/bss.cpp.o", ".bss")], ".bss .bss.* COMMON"),
]


def main():
    out = Path(sys.argv[1])
    src_objects = sys.argv[2:]
    retail = {obj for _, _, inputs, _ in SECTIONS for obj, _ in inputs}
    missing = sorted(obj for obj in retail if obj not in src_objects)
    if missing:
        raise SystemExit(f"The retail data's objects aren't built: {', '.join(missing)}")

    lines = ["OUTPUT_ARCH(mips:5900)", "ENTRY(entry)", "", "SECTIONS", "{", "    .text 0x100000 :", "    {"]
    for obj in src_objects:
        lines.append(f"        {obj}(.text .text.*);")

    lines.append("        *(.text .text.*);")
    lines.append("    }")
    for section, align, inputs, src_inputs in SECTIONS:
        noload = " (NOLOAD)" if section in (".sbss", ".bss") else ""
        lines.append(f"    {section} ALIGN({align}){noload} :")
        lines.append("    {")
        for obj, input_section in inputs:
            lines.append(f"        {obj}({input_section});")

        if src_inputs:
            for obj in src_objects:
                if obj not in retail:
                    lines.append(f"        {obj}({src_inputs});")

            lines.append(f"        *({src_inputs});")

        lines.append("    }")
        if section == ".sdata":
            lines.append("    _gp = ADDR(.sdata) + 0x7FF0;")
        elif section == ".sbss":
            lines.append("    _fbss = ADDR(.sbss);")

    # The heap starts at _end (sbrk's first break): aligned like the retail one (0x3DB200)
    lines.append("    _end = ALIGN(0x200);")

    lines += ["    /DISCARD/ :", "    {", "        *(.reginfo) *(.MIPS.abiflags) *(.pdr) *(.comment) *(.gnu.attributes) *(.mdebug.*) *(.note.*) *(.gnu.lto_*)", "    }", "}"]
    out.write_text("\n".join(lines) + "\n", newline="\n")


main()
