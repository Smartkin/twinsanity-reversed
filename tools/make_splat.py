#!/usr/bin/env python3
"""Writes build/splat.yaml: splat.yaml with its .text split into a subsegment per function of symbol_addrs.txt, so every
function is an asm file of its own (asm/text/<name>.s) that a C++ definition in src/ can take the place of.

    tools/make_splat.py [--inner]

--inner also splits at the functions spimdisasm found inside another function's subsegment in the asm/ there is (func_<address>,
the ones symbol_addrs.txt doesn't have): split.sh splits twice. A file is linked whole or not at all, so a function replaced in
src/ took the functions after it in its file along, and the linker script made them 0 (a vtable's methods, a qsort comparator).
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
TEXT_FILE_OFFSET = 0x1000
TEXT_VRAM = 0x100000
TEXT_END = 0x2D9D88


def inner_functions():
    found = []
    for path in sorted((HERE / "asm" / "text").glob("*.s")):
        for match in re.finditer(r"^\s*glabel\s+\"?(func_([0-9A-F]{8}))\b", path.read_text(), re.MULTILINE):
            if match.group(1) != path.stem:
                found.append((int(match.group(2), 16), match.group(1)))

    return found


def main():
    template = (HERE / "splat.yaml").read_text()
    functions = []
    for line in (HERE / "symbol_addrs.txt").read_text().splitlines():
        match = re.match(r"(\w+) = 0x([0-9A-Fa-f]+); // type:func", line) or re.match(r"(D_[0-9A-F]{8}) = 0x([0-9A-Fa-f]+);", line)
        if match and (match.group(1).startswith("D_") is False or TEXT_VRAM <= int(match.group(2), 16) < TEXT_END):
            address = int(match.group(2), 16)
            if TEXT_VRAM <= address < TEXT_END:
                functions.append((address, match.group(1)))

    if "--inner" in sys.argv[1:]:
        known = {address for address, _ in functions}
        functions += [(address, name) for address, name in inner_functions() if address not in known]

    functions.sort()
    if not functions or functions[0][0] != TEXT_VRAM:
        functions.insert(0, (TEXT_VRAM, "text_start"))

    entries = "\n".join(f"      - [0x{address - TEXT_VRAM + TEXT_FILE_OFFSET:X}, asm, text/{name}]" for address, name in functions)
    marker = "      - [0x1000, asm, text]"
    if marker not in template:
        raise SystemExit(f"splat.yaml has no {marker!r} to split")

    generated = template.replace(marker, entries)
    generated = generated.replace("base_path: .", f"base_path: {HERE.as_posix()}")
    out = HERE / "build" / "splat.yaml"
    out.parent.mkdir(exist_ok=True)
    out.write_text(generated, newline="\n")
    print(f"{out}: {len(functions)} functions")


main()
