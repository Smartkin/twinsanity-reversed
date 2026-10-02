#!/usr/bin/env python3
"""Writes symbol_addrs.txt for splat from a Ghidra export (tools/ghidra/ExportProgramInfo.java, written to ghidra/).

Functions keep their Ghidra names, Ghidra's own FUN_ names included, so the asm, the notes and the Ghidra project name the
same thing. Data labels are the ones somebody named (user defined or imported), Ghidra's automatic DAT_/PTR_/s_ labels are left
to spimdisasm, which makes its own. Names become valid symbols: "?" (a guess) turns into "_", a destructor's "~X" into "X_dtor",
anything else that isn't a letter, digit or underscore into "_", and a name used twice gets the address after it.

    tools/make_symbols.py [ghidra export folder] [output]
"""
import csv
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
EXPORT = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "ghidra"
OUTPUT = Path(sys.argv[2]) if len(sys.argv) > 2 else HERE / "symbol_addrs.txt"

# The loaded sections, from the ELF's section headers
TEXT = (0x100000, 0x2D9D88)
DATA_SECTIONS = [(".data", 0x2E6F00, 0x2EC348), (".vudata", 0x2EC348, 0x2EC3D2), (".rodata", 0x2EC400, 0x309868),
                 (".sdata", 0x309880, 0x30A460), (".sbss", 0x30A480, 0x30ACD8), (".bss", 0x30AD00, 0x3DB200)]
# Data in the middle of .text: a split point each, so the code around it is disassembled as code (spimdisasm writes a function
# with data in it as words, which don't move with what they point at). Sony's libmpeg keeps a 16 byte mask after _copyAddRefImage's
# helper, then a function of two instructions
TEXT_DATA = {0x2BF5B0: "D_002BF5B0"}
TEXT_FUNCTIONS = {0x2BF5C0: "FUN_002bf5c0"}
ELF_PATH = HERE / "SLES_525.68"

# Sony SDK functions Ghidra named otherwise (FUN_, FID_conflict_ for the ones its function IDs couldn't tell apart, or a guess),
# by their SDK names, which PS2SDK takes the place of them by
SDK_NAMES = {
    0x2C2350: "sceGsResetPath",
    0x2C2AA8: "sceDevVu0Reset",
    0x2C2B88: "sceDevVu1Reset",
    0x2C2C58: "sceDevVif0Reset",
    0x2C2C90: "sceDevVif1Reset",
    0x2C2CC8: "scePadInit",
    0x2C3C78: "scePadInfoPressMode",
    0x2C3D48: "scePadGetModVersion",
    0x2C4E18: "sceCdGetError",
    0x2C5130: "sceCdStStart",
    0x2C5170: "sceCdStSeek",
    0x2C51A0: "sceCdStStop",
    0x2C5920: "sceMcInit",
    0x2C5CC0: "sceMcClose",
    0x2C6680: "sceMcFormat",
    0x2D3CC8: "sceFsReset",
}

# Case bodies of a switch Ghidra made functions of (GetButtonPressure's, one per button): labels of the function they're in,
# a file each would branch into the others' labels
CASE_BODIES = [(0x2B2278, 0x2B27D0)]
AUTOMATIC = re.compile(r"^(DAT|PTR|LAB|FUN|SUB|UNK|OFF|BYTE|WORD|DWORD|QWORD|FLOAT|DOUBLE|s|u|switchdata|switchD|caseD|thunk|EXT)_")


def symbol_name(name, address):
    name = name.replace("?", "_")
    if name.startswith("~"):
        name = name[1:] + "_dtor"
    name = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if not name or name[0].isdigit():
        name = "_" + name
    return name


def read_tsv(path):
    with open(path, encoding="utf-8") as file:
        return list(csv.DictReader(file, delimiter="\t", quoting=csv.QUOTE_NONE))


def main():
    functions = read_tsv(EXPORT / "functions.tsv")
    symbols = read_tsv(EXPORT / "symbols.tsv")
    data = {int(row["address"], 16): row for row in read_tsv(EXPORT / "data.tsv") if re.fullmatch(r"[0-9a-f]{8}", row["address"])}

    entries = {}
    for row in functions:
        address = int(row["address"], 16)
        if TEXT[0] <= address < TEXT[1]:
            case_body = any(start <= address < end for start, end in CASE_BODIES)
            entries[address] = (SDK_NAMES.get(address, row["name"]), "jtbl_label" if case_body else "func", None)

    for row in symbols:
        if row["type"] != "Label" or row["source"] not in ("USER_DEFINED", "IMPORTED") or row["primary"] != "true":
            continue

        if not re.fullmatch(r"[0-9a-f]{8}", row["address"]) or AUTOMATIC.match(row["name"]):
            continue

        address = int(row["address"], 16)
        section = next((name for name, start, end in DATA_SECTIONS if start <= address < end), None)
        if section is None or address in entries:
            continue

        item = data.get(address)
        size = int(item["length"]) if item and item["length"].isdigit() else None
        entries[address] = (row["name"], "data", size)

    # Where .bss ends is where its last array ends: FUN_002941b0 bounds its blend shape allocator with it (the entry, which used it
    # as the heap's start, is src/crt0.cpp's, which takes the linker's _end)
    entries[0x3DB200] = ("G_BLEND_SHAPE_FLOATS_end", "data", None)

    for address, name in TEXT_FUNCTIONS.items():
        entries.setdefault(address, (name, "func", None))

    for address, name in TEXT_DATA.items():
        entries[address] = (name, "text_data", None)

    # Ghidra's switch tables (switchdataD_ labels, their cases typed as pointers) are jump tables: spimdisasm makes labels of their
    # cases in the function and writes the entries as those labels
    rows = sorted((row for row in read_tsv(EXPORT / "data.tsv") if re.fullmatch(r"[0-9a-f]{8}", row["address"])), key=lambda row: row["address"])
    by_address = {int(row["address"], 16): row for row in rows}
    jump_table_words = set()
    for row in symbols:
        if not row["name"].startswith("switchdataD_") or not re.fullmatch(r"[0-9a-f]{8}", row["address"]):
            continue

        start = int(row["address"], 16)
        end = start
        while end in by_address and by_address[end]["type"] == "pointer" and (end == start or not by_address[end]["label"]):
            jump_table_words.add(end)
            end += 4

        if end > start:
            entries[start] = (row["name"], "jtbl", end - start)

    # Every other word Ghidra typed as a pointer (or an array of them) gets a symbol where it points, so spimdisasm writes the
    # word as that symbol and it follows what it points at. Adjacent words pointing between the same two function starts are a
    # switch's table Ghidra didn't label (BuildScriptCommand's), their cases labels of the function; any other code address
    # that starts no function is a function of its own (a virtual method or a callback: vtables have a word between their
    # entries). A data address gets a label
    import bisect
    elf = ELF_PATH.read_bytes()
    starts = sorted(address for address, (_, kind, _) in entries.items() if kind == "func")

    def region(target):
        return bisect.bisect_right(starts, target) - 1

    pointer_words = {}
    for row in rows:
        kind = row["type"]
        array = re.search(r"\*\[(\d+)\]$", kind)
        count = int(array.group(1)) if array else 1 if kind == "pointer" or kind.endswith("*") or kind.endswith("*32") else 0
        for i in range(count):
            pointer_words[int(row["address"], 16) + 4 * i] = True

    def body_of(target, word_address):
        # A case when a neighbouring word points between the same two function starts
        if target in entries and entries[target][1] == "func":
            return None

        for neighbour in (word_address - 4, word_address + 4):
            if neighbour in pointer_words:
                offset = neighbour - 0x100000 + 0x1000
                other = struct.unpack_from("<I", elf, offset)[0] if 0 <= offset <= len(elf) - 4 else 0
                if TEXT[0] <= other < TEXT[1] and region(other) == region(target):
                    return region(target)

        return None

    elf = ELF_PATH.read_bytes()
    table_start, table_body, table_end = None, None, None

    def close_table():
        if table_start is not None and table_start not in entries:
            entries[table_start] = (f"jtbl_{table_start:08X}", "jtbl", table_end - table_start)

    for row in rows:
        if not re.fullmatch(r"[0-9a-f]{8}", row["address"]):
            continue

        kind = row["type"]
        array = re.search(r"\*\[(\d+)\]$", kind)
        if array:
            count = int(array.group(1))
        elif kind == "pointer" or kind.endswith("*") or kind.endswith("*32"):
            count = 1
        else:
            continue

        address = int(row["address"], 16)
        for i in range(count):
            offset = address + 4 * i - 0x100000 + 0x1000
            if not 0 <= offset <= len(elf) - 4:
                break

            target = struct.unpack_from("<I", elf, offset)[0]
            word_address = address + 4 * i
            if word_address in jump_table_words or target % 4:
                continue

            body = body_of(target, word_address) if TEXT[0] <= target < TEXT[1] else None
            if body is not None:
                if table_start is not None and table_body == body and table_end == word_address:
                    table_end += 4
                else:
                    close_table()
                    table_start, table_body, table_end = word_address, body, word_address + 4

                continue

            if target in entries:
                continue

            if TEXT[0] <= target < TEXT[1]:
                entries[target] = (f"FUN_{target:08x}", "func", None)
            elif any(start <= target < end for _, start, end in DATA_SECTIONS) or 0x2D9D90 <= target < 0x2E6ED0:
                entries[target] = (f"D_{target:08X}", "data", None)

    close_table()

    # Code addresses in data Ghidra didn't type: a function when its first instruction can start one (a prologue, an empty or
    # constant stub, a constructor loading its vtable, a getter reading from this) and a known function's address is within 32
    # bytes of the word, which makes it a vtable or a table of callbacks (strings, packed shorts and sizes aren't next to any)
    def can_start_function(target):
        first = struct.unpack_from("<I", elf, target - 0x100000 + 0x1000)[0]
        opcode, rs, rt = first >> 26, first >> 21 & 0x1F, first >> 16 & 0x1F
        return (first >> 16 == 0x27BD and first & 0x8000 != 0) or first == 0x03E00008 \
            or (opcode == 0x0F and rt in (2, 3) and 0x2E <= first & 0xFFFF <= 0x31) \
            or (opcode in (0x20, 0x21, 0x23, 0x24, 0x25, 0x31, 0x37) and rs == 4)

    known_functions = {address for address, (_, kind, _) in entries.items() if kind == "func"}
    words = {}
    for _, start, end in DATA_SECTIONS[:4]:
        for address in range(start, end - 3, 4):
            words[address] = struct.unpack_from("<I", elf, address - 0x100000 + 0x1000)[0]

    for address, target in words.items():
        if target in entries or target % 4 or not TEXT[0] <= target < TEXT[1] or address in jump_table_words:
            continue

        neighbours = any(words.get(address + delta) in known_functions for delta in range(-32, 36, 4) if delta)
        first = struct.unpack_from("<I", elf, target - 0x100000 + 0x1000)[0]
        prologue = first >> 16 == 0x27BD and first & 0x8000 != 0 and target & 0xFFF != 0
        if (neighbours or prologue) and can_start_function(target):
            entries[target] = (f"FUN_{target:08x}", "func", None)

    # Strings with a word that looks like an address of the segment: spimdisasm would write the word as a symbol (an
    # address past the string's end, or a variable's), which moves when the layout does and changes the string's bytes
    # ("SLES_525.68", "Level01" to "Level13"). They're strings to spimdisasm
    segment = (TEXT[0], DATA_SECTIONS[-1][2])
    for row in read_tsv(EXPORT / "rawstrings.tsv"):
        if not re.fullmatch(r"[0-9a-f]{8}", row["address"]) or len(row["string"] or "") < 4:
            continue

        address = int(row["address"], 16)
        if not any(start <= address < end for _, start, end in DATA_SECTIONS[:4]):
            continue

        end = address + len(row["string"].encode("latin-1", "replace")) + 1
        word = (address + 3) & ~3
        looks_like_address = False
        while word + 4 <= end:
            value = struct.unpack_from("<I", elf, word - 0x100000 + 0x1000)[0]
            looks_like_address |= segment[0] <= value <= segment[1]
            word += 4

        if looks_like_address:
            name = entries[address][0] if address in entries else f"D_{address:08X}"
            entries[address] = (name, "asciz", None)

    used = {}
    lines = []
    for address in sorted(entries):
        name, kind, size = entries[address]
        symbol = symbol_name(name, address)
        if symbol in used:
            symbol = f"{symbol}_{address:08X}"

        used[symbol] = address
        # Data gets its size only, splat's types would make spimdisasm read it as that type
        attributes = ([f"type:{kind}"] if kind not in ("data", "text_data") else []) + ([f"size:0x{size:X}"] if size else [])
        lines.append(f"{symbol} = 0x{address:08X};" + (f" // {' '.join(attributes)}" if attributes else ""))

    OUTPUT.write_text("\n".join(lines) + "\n", newline="\n")
    print(f"{OUTPUT}: {len(lines)} symbols")


main()
