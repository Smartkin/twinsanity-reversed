#!/usr/bin/env python3
"""Compares two builds of the PS2 executable (build/SLES_525.68.elf, linked with --emit-relocs).

    tools/compare_builds.py A.elf B.elf            the same program, laid out the same or not
    tools/compare_builds.py A.elf B.elf --bytes    the same bytes at the same addresses

The first compares every function and object both have by name, each relocated field read as what it points at (an object or
function + offset; a constant nothing names by its 4 bytes; a jump table by where its first case goes) instead of an address, so
code moved between files, sections grown or shrunk and objects reordered still compare the same. Prints the names that differ
(--show N: the first field that differs in N of them). The second compares what the executables load (each section's address,
size and bytes) and every symbol's address: the test that a change moved nothing.
"""
import bisect
import sys
from collections import defaultdict

from elftools.elf.elffile import ELFFile
from elftools.elf.relocation import RelocationSection

# The relocations' types and the bits of their field that hold the address
R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16, R_MIPS_GPREL16 = 2, 4, 5, 6, 7
ADDRESS_BITS = {R_MIPS_32: 0xFFFFFFFF, R_MIPS_26: 0x03FFFFFF, R_MIPS_HI16: 0xFFFF, R_MIPS_LO16: 0xFFFF, R_MIPS_GPREL16: 0xFFFF}
SHF_ALLOC = 0x2


class Build:
    def __init__(self, path):
        self.file = open(path, "rb")
        self.elf = ELFFile(self.file)
        self.symbols = []
        for symbol in self.elf.get_section_by_name(".symtab").iter_symbols():
            if (symbol["st_info"]["type"] in ("STT_FUNC", "STT_OBJECT") and symbol.name
                    and symbol["st_shndx"] not in ("SHN_UNDEF", "SHN_ABS")):
                self.symbols.append((symbol.name, symbol["st_value"], symbol["st_size"]))

        self.spans = sorted((value, value + max(size, 1), name) for name, value, size in self.symbols)
        self.starts = [span[0] for span in self.spans]
        self.sections = [section for section in self.elf.iter_sections() if section["sh_flags"] & SHF_ALLOC]
        # Every relocated field: its address, type, symbol (or the section's name) and addend (or address)
        self.relocations = {}
        for section in self.elf.iter_sections():
            if not isinstance(section, RelocationSection):
                continue

            if not self.elf.get_section(section["sh_info"])["sh_flags"] & SHF_ALLOC:
                continue

            symbols = self.elf.get_section(section["sh_link"])
            for relocation in section.iter_relocations():
                symbol = symbols.get_symbol(relocation["r_info_sym"])
                addend = relocation.entry.get("r_addend", 0)
                if symbol["st_info"]["type"] == "STT_SECTION" or not symbol.name:
                    target = self.elf.get_section(symbol["st_shndx"])
                    self.relocations[relocation["r_offset"]] = (relocation["r_info_type"], None, target["sh_addr"] + addend)
                else:
                    self.relocations[relocation["r_offset"]] = (relocation["r_info_type"], symbol.name, addend)

        if not self.relocations:
            raise SystemExit(f"{path} has no relocations: link it with --emit-relocs")

    def contents(self, address, size):
        for section in self.sections:
            if section["sh_addr"] <= address < section["sh_addr"] + section["sh_size"]:
                if section["sh_type"] == "SHT_NOBITS":
                    return None

                offset = address - section["sh_addr"]
                return section.data()[offset:offset + size]

        return None

    def name_of(self, address):
        """What an address points at, the same in any layout: an object it's in, else what's there"""
        index = bisect.bisect_right(self.starts, address) - 1
        while index >= 0 and self.starts[index] == self.spans[index][0]:
            start, end, _ = self.spans[index]
            if start <= address < end:
                names = sorted(name for s, _, name in self.spans if s == start)
                return names[0], address - start

            if index == 0 or self.starts[index - 1] != start:
                break

            index -= 1

        relocated = self.relocations.get(address)
        if relocated is not None:
            _, name, addend = relocated
            if name is None:
                index = bisect.bisect_right(self.starts, addend) - 1
                inside = index >= 0 and self.spans[index][0] <= addend < self.spans[index][1]
                name, addend = (self.spans[index][2], addend - self.spans[index][0]) if inside else ("?", 0)

            return "table:" + name, addend

        data = self.contents(address, 4)
        return "const:" + (data.hex() if data is not None else "nobits"), 0

    def normalized(self, address, size):
        data = self.contents(address, size)
        if data is None:
            return ("nobits", size), ()

        data = bytearray(data)
        fields = []
        for offset in range(0, size, 4):
            relocated = self.relocations.get(address + offset)
            if relocated is None:
                continue

            kind, name, addend = relocated
            if name is None:
                name, addend = self.name_of(addend)

            fields.append((offset, kind, name, addend))
            word = int.from_bytes(data[offset:offset + 4], "little") & ~ADDRESS_BITS.get(kind, 0xFFFFFFFF)
            data[offset:offset + 4] = word.to_bytes(4, "little")

        return bytes(data), tuple(fields)

    def by_name(self):
        names = defaultdict(list)
        for name, value, size in self.symbols:
            names[name].append((value, size))

        return names


def compare_programs(a, b, show):
    names_a, names_b = a.by_name(), b.by_name()
    different = []
    for name in sorted(set(names_a) & set(names_b)):
        if sorted(a.normalized(v, s) for v, s in names_a[name]) != sorted(b.normalized(v, s) for v, s in names_b[name]):
            different.append(name)

    only_a = sorted(set(names_a) - set(names_b))
    only_b = sorted(set(names_b) - set(names_a))
    for name in different[:show]:
        for (data_a, fields_a), (data_b, fields_b) in zip(sorted(a.normalized(v, s) for v, s in names_a[name]),
                                                          sorted(b.normalized(v, s) for v, s in names_b[name])):
            if data_a != data_b:
                print(f"  {name}: its bytes differ")
            for field_a, field_b in zip(fields_a, fields_b):
                if field_a != field_b:
                    print(f"  {name}: {field_a} against {field_b}")
                    break

    print(f"{len(set(names_a) & set(names_b))} names compared, {len(different)} differ, {len(only_a)} only in the first, "
          f"{len(only_b)} only in the second")
    for name in different[:40]:
        print(f"  differs: {name}")
    for label, names in (("only in the first", only_a), ("only in the second", only_b)):
        if names:
            print(f"  {label}: {' '.join(names[:40])}")

    same = not different and not only_a and not only_b
    print("THE SAME PROGRAM" if same else "DIFFERENT")
    return same


def compare_bytes(a, b):
    same = True
    sections_a = {s.name: s for s in a.sections}
    sections_b = {s.name: s for s in b.sections}
    for name in sorted(set(sections_a) | set(sections_b)):
        if name not in sections_a or name not in sections_b:
            print(f"{name}: only in the {'first' if name in sections_a else 'second'}")
            same = False
            continue

        x, y = sections_a[name], sections_b[name]
        if (x["sh_addr"], x["sh_size"]) != (y["sh_addr"], y["sh_size"]):
            print(f"{name}: 0x{x['sh_addr']:X}+0x{x['sh_size']:X} against 0x{y['sh_addr']:X}+0x{y['sh_size']:X}")
            same = False
        elif x["sh_type"] != "SHT_NOBITS" and x.data() != y.data():
            print(f"{name}: its bytes differ")
            same = False

    addresses_a = {name: value for name, value, _ in a.symbols}
    addresses_b = {name: value for name, value, _ in b.symbols}
    moved = sorted(name for name in set(addresses_a) & set(addresses_b) if addresses_a[name] != addresses_b[name])
    for name in moved[:20]:
        print(f"  moved: {name} 0x{addresses_a[name]:X} -> 0x{addresses_b[name]:X}")

    same = same and not moved
    print("THE SAME BYTES" if same else "DIFFERENT")
    return same


def main():
    arguments = sys.argv[1:]
    show = int(arguments[arguments.index("--show") + 1]) if "--show" in arguments else 0
    a, b = Build(arguments[0]), Build(arguments[1])
    same = compare_bytes(a, b) if "--bytes" in arguments else compare_programs(a, b, show)
    sys.exit(0 if same else 1)


if __name__ == "__main__":
    main()
