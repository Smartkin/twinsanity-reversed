#!/usr/bin/env python3
"""Compares what a built ELF loads into memory with what the retail one loads: the same bytes at the same addresses, and the
same memory size. Prints the first differences with the function or data they're in (from the map file).

    tools/compare.py <built elf> <retail elf> [map file] [--touch FILE]

--touch writes FILE when they match (ninja's stamp of the check).
"""
import bisect
import re
import struct
import sys


def load_image(path):
    data = open(path, "rb").read()
    phoff, = struct.unpack_from("<I", data, 0x1C)
    phentsize, phnum = struct.unpack_from("<HH", data, 0x2A)
    image = {}
    memory_end = 0
    for i in range(phnum):
        kind, offset, vaddr, _, filesz, memsz = struct.unpack_from("<6I", data, phoff + i * phentsize)
        if kind != 1:
            continue

        image[vaddr] = data[offset:offset + filesz]
        memory_end = max(memory_end, vaddr + memsz)

    start = min(image)
    end = max(vaddr + len(chunk) for vaddr, chunk in image.items())
    flat = bytearray(end - start)
    for vaddr, chunk in image.items():
        flat[vaddr - start:vaddr - start + len(chunk)] = chunk

    return start, bytes(flat), memory_end


def symbols(map_path):
    names = []
    for line in open(map_path, encoding="utf-8", errors="replace"):
        match = re.match(r"\s+0x([0-9a-f]{8,16})\s+([A-Za-z_][\w.]*)\s*$", line)
        if match:
            names.append((int(match.group(1), 16), match.group(2)))

    names.sort()
    return names


def main():
    arguments = sys.argv[1:]
    stamp = None
    if "--touch" in arguments:
        index = arguments.index("--touch")
        stamp = arguments[index + 1]
        del arguments[index:index + 2]

    built_start, built, built_end = load_image(arguments[0])
    retail_start, retail, retail_end = load_image(arguments[1])
    names = symbols(arguments[2]) if len(arguments) > 2 else []
    addresses = [address for address, _ in names]

    def where(address):
        index = bisect.bisect_right(addresses, address) - 1
        return f"{names[index][1]}+0x{address - names[index][0]:X}" if index >= 0 else "?"

    problems = []
    if built_start != retail_start:
        problems.append(f"the image starts at {built_start:X}, the retail one at {retail_start:X}")

    if len(built) != len(retail):
        problems.append(f"the image is 0x{len(built):X} bytes, the retail one 0x{len(retail):X}")

    if built_end != retail_end:
        problems.append(f"memory ends at {built_end:X}, the retail one at {retail_end:X}")

    differences = [i for i in range(min(len(built), len(retail))) if built[i] != retail[i]]
    for i in differences[:20]:
        address = retail_start + i
        problems.append(f"{address:08X} ({where(address)}): {built[i]:02X}, retail {retail[i]:02X}")

    if differences:
        problems.append(f"{len(differences)} bytes differ")

    if problems:
        print("\n".join(problems))
        sys.exit(1)

    print(f"OK: the load image matches the retail one (0x{len(retail):X} bytes at {retail_start:X}, memory to {retail_end:X})")
    if stamp:
        open(stamp, "w").close()


if __name__ == "__main__":
    main()
