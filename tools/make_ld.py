#!/usr/bin/env python3
"""Writes the linker script: the functions' asm in the retail order, then what src/ compiles and PS2SDK's libraries, then the
retail data.

Each section follows the one before it at the alignment that puts it where the retail ELF has it (.vutext 16, the rest 64), so
with nothing from src/ the load segment comes out byte for byte the retail one, and with it everything moves along. A function src/
defines takes its asm's place: its asm object isn't linked. Addresses splat couldn't name (the VU programs' entries inside
.vutext, the middle of a function or a data blob) are defined from the section, function or file they're in, so they move with
it; addresses outside the program (hardware registers, uncached and scratchpad memory) stay as they are. The Sony SDK functions
ps2sdk.txt lists aren't linked either: PS2SDK's libraries take their place, their sections go after src/'s. Neither are the
game's functions retired.txt lists, whose work the C++ does elsewhere, nor fragments.txt's bytes between functions that nothing
reaches.

    tools/make_ld.py <output> [--nm NM] [--matching] [--shift N] [src objects...]

--nm is the toolchain's nm, which tells what src/'s objects define (tools/local_config.py's by default).

--matching leaves ps2sdk.txt, retired.txt and fragments.txt out: every retail function is linked, as the retail executable has them.

--shift puts N bytes before the first function, moving everything: the test that nothing depends on where things are.
"""
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "tools"))
import local_config

NM = local_config.ee_tool("nm")

# The retail layout: (output section, address, [objects' input sections])
DATA_SECTIONS = [
    (".vutext", 0x2D9D90, 0x2E6ED0, 16, [("build/asm/data/vutext.s.o", ".text")], ".vutext"),
    (".data", 0x2E6F00, 0x2EC3D2, 64, [("build/asm/data/data.data.s.o", ".data"), ("build/asm/data/vudata.data.s.o", ".data")], ".data"),
    (".rodata", 0x2EC400, 0x309868, 64, [("build/asm/data/rodata.rodata.s.o", ".rodata")], ".rodata"),
    (".sdata", 0x309880, 0x30A460, 64, [("build/asm/data/sdata.sdata.s.o", ".sdata")], ".sdata"),
    (".sbss", 0x30A480, 0x30ACD8, 64, [("build/asm/data/sbss.sbss.s.o", ".sbss")], ".sbss"),
    (".bss", 0x30AD00, 0x3DB200, 64, [("build/asm/data/bss.bss.s.o", ".bss")], ".bss"),
]
SRC_INPUTS = {".vutext": None, ".data": ".data .data.*", ".rodata": ".rodata .rodata.*", ".sdata": ".sdata .sdata.*",
              ".sbss": ".sbss .sbss.* .scommon", ".bss": ".bss .bss.* COMMON"}
LINKER_DEFINED = {"_gp", "_end"}


def text_functions():
    functions = []
    for line in (HERE / "build" / "splat.yaml").read_text().splitlines():
        match = re.match(r"\s+- \[0x([0-9A-F]+), asm, text/(\w+)\]", line)
        if match:
            functions.append((int(match.group(1), 16) - 0x1000 + 0x100000, match.group(2)))

    return functions


def defined_functions(objects):
    """What src/'s objects define: functions, and data that the asm kept in .text (libmpeg's lq mask D_002BF5B0)"""
    if not objects:
        return set()

    output = subprocess.run([NM, "--defined-only", "-g", *objects], capture_output=True, text=True, check=True).stdout
    return {parts[2] for parts in (line.split() for line in output.splitlines()) if len(parts) == 3 and parts[1] in "TWRD"}


def dropped_functions():
    """ps2sdk.txt's Sony SDK functions (PS2SDK has them) and retired.txt's game functions (the C++ does their work elsewhere,
    the platform layer's start-up for instance), fragments.txt's unreachable bytes, and ps2sdk.txt's aliases"""
    names, aliases = set(), {}
    for file in ("ps2sdk.txt", "retired.txt", "fragments.txt"):
        for line in (HERE / file).read_text().splitlines():
            line = line.split("#", 1)[0].strip()
            if "=" in line:
                name, target = (part.strip() for part in line.split("=", 1))
                aliases[name] = target
            elif line:
                names.add(line)

    return names, aliases


def main():
    global NM
    out = Path(sys.argv[1])
    src_objects = sys.argv[2:]
    if src_objects[:1] == ["--nm"]:
        NM = src_objects[1]
        src_objects = src_objects[2:]

    shift = 0
    matching = src_objects[:1] == ["--matching"]
    if matching:
        src_objects = src_objects[1:]

    if src_objects[:1] == ["--shift"]:
        shift = int(src_objects[1], 0)
        src_objects = src_objects[2:]

    sdk_replaced, aliases = dropped_functions() if not matching else (set(), {})
    replaced = defined_functions(src_objects) | sdk_replaced
    functions = text_functions()
    starts = [address for address, _ in functions]

    lines = ["OUTPUT_ARCH(mips:5900)", "ENTRY(entry)", "", "SECTIONS", "{", "    .text 0x100000 :", "    {"]
    if shift:
        lines.append(f"        . += 0x{shift:X};")

    for _, name in functions:
        if name not in replaced:
            lines.append(f"        build/asm/text/{name}.s.o(.text);")

    for obj in src_objects:
        lines.append(f"        {obj}(.text .text.*);")

    # Not the VU programs: splat's .vutext is a .text section of its own, placed after
    lines.append("        *(EXCLUDE_FILE(build/asm/data/*) .text .text.*);")
    lines.append("    }")
    for section, _, _, align, inputs, _ in DATA_SECTIONS:
        noload = " (NOLOAD)" if section in (".sbss", ".bss") else ""
        lines.append(f"    {section} ALIGN({align}){noload} :")
        lines.append("    {")
        for obj, input_section in inputs:
            lines.append(f"        {obj}({input_section});")

        if SRC_INPUTS[section]:
            for obj in src_objects:
                lines.append(f"        {obj}({SRC_INPUTS[section]});")

            lines.append(f"        *({SRC_INPUTS[section]});")

        lines.append("    }")
        if section == ".sdata":
            lines.append("    _gp = ADDR(.sdata) + 0x7FF0;")
        elif section == ".sbss":
            lines.append("    _fbss = ADDR(.sbss);")

    # The heap starts at _end (sbrk's first break): aligned like the retail one (0x3DB200)
    lines.append("    _end = ALIGN(0x200);")
    # Data stays when the function it points into is replaced: its jump tables' cases become nothing, unless something else has
    # them. Any other label in its file would be a function or data something else may still use (split.sh gives every function
    # a file of its own): that has to be src/'s or the link fails, never 0
    for name in sorted(replaced):
        path = HERE / "asm" / "text" / f"{name}.s"
        if path.exists():
            for kind, label in re.findall(r"^\s*([gjad]label)\s+\"?([\w.]+)", path.read_text(), re.MULTILINE):
                if label == name:
                    continue
                if kind != "jlabel":
                    raise SystemExit(f"{path.name} has {label} besides {name}, which src/ replaces: run tools/split.sh")

                lines.append(f'    PROVIDE("{label}" = 0);')

    for name, target in sorted(aliases.items()):
        lines.append(f"    PROVIDE({name} = {target});")

    # What splat couldn't name, unless a label of the asm has the name after all
    labels = set()
    for path in (HERE / "asm").rglob("*.s"):
        labels.update(re.findall(r"^\s*[gdjae]label \"?(\w+)", path.read_text(), re.MULTILINE))

    for line in (HERE / "build" / "undefined_syms_auto.txt").read_text().splitlines():
        match = re.match(r"(\w+) = 0x([0-9A-Fa-f]+);", line)
        if not match or match.group(1) in LINKER_DEFINED or match.group(1) in labels:
            continue

        name, address = match.group(1), int(match.group(2), 16)
        # An address at a section's end (the end of its last object) stays at the end of the retail part of it
        section = next((entry for entry in DATA_SECTIONS if entry[1] <= address <= entry[2]), None)
        if section is not None:
            lines.append(f"    {name} = ADDR({section[0]}) + 0x{address - section[1]:X};")
        elif 0x100000 <= address < 0x2D9D88:
            index = max(i for i, start in enumerate(starts) if start <= address)
            function_address, function_name = functions[index]
            if function_name in replaced:
                raise SystemExit(f"{name} points into {function_name}, which src/ replaces")

            lines.append(f"    {name} = {function_name} + 0x{address - function_address:X};")
        else:
            lines.append(f"    {name} = 0x{address:X};")

    lines += ["    /DISCARD/ :", "    {", "        *(.reginfo) *(.MIPS.abiflags) *(.pdr) *(.comment) *(.gnu.attributes) *(.mdebug.*) *(.note.*) *(.gnu.lto_*)", "    }", "}"]
    out.write_text("\n".join(lines) + "\n", newline="\n")


main()
