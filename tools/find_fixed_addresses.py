#!/usr/bin/env python3
"""Finds program addresses a moved build still has where they were: links a copy with everything shifted (make_ld.py --shift)
and compares it with the retail image. A data word that is a program address and didn't move with it, or a lui/addiu or lui/ori
pair building one, is a pointer spimdisasm left as a number.

    tools/find_fixed_addresses.py <shifted elf> <retail elf> <shift> [map]
"""
import bisect
import re
import struct
import sys

sys.path.insert(0, __import__("os").path.dirname(__file__))
from compare import load_image

PROGRAM = (0x100000, 0x3DB200)


def main():
    shifted_start, shifted, _ = load_image(sys.argv[1])
    retail_start, retail, _ = load_image(sys.argv[2])
    shift = int(sys.argv[3], 0)
    names = []
    if len(sys.argv) > 4:
        for line in open(sys.argv[4]):
            match = re.match(r"\s+0x([0-9a-f]{8,16})\s+([A-Za-z_][\w.]*)\s*$", line)
            if match:
                names.append((int(match.group(1), 16) - shift, match.group(2)))
        names.sort()
    addresses = [a for a, _ in names]

    def where(address):
        index = bisect.bisect_right(addresses, address) - 1
        return f"{names[index][1]}+0x{address - names[index][0]:X}" if index >= 0 else "?"

    def word(image, start, address):
        offset = address - start
        return struct.unpack_from("<I", image, offset)[0] if 0 <= offset <= len(image) - 4 else None

    found = []
    # Data words
    for address in range(0x2E6F00, 0x30A460, 4):
        old = word(retail, retail_start, address)
        new = word(shifted, shifted_start, address + shift)
        if old is not None and PROGRAM[0] <= old < PROGRAM[1] and new == old:
            found.append((address, f"data word {old:08X}"))

    # Code: a lui of a program address's upper half whose immediate didn't move while the pair's addiu/ori didn't either
    for address in range(0x100000, 0x2D9D88, 4):
        old = word(retail, retail_start, address)
        new = word(shifted, shifted_start, address + shift)
        if old is None or new is None or old >> 26 != 0x0F:
            continue

        high = old & 0xFFFF
        if not (0x10 <= high <= 0x3E):
            continue

        register = old >> 16 & 0x1F
        for next_address in range(address + 4, min(address + 64, 0x2D9D88), 4):
            follow = word(retail, retail_start, next_address)
            opcode = follow >> 26
            if opcode in (0x09, 0x0D, 0x23, 0x2B, 0x20, 0x24, 0x21, 0x25, 0x28, 0x29, 0x31, 0x39, 0x37, 0x3F, 0x1E, 0x1F) and follow >> 21 & 0x1F == register:
                low = follow & 0xFFFF
                value = (high << 16) + (low if opcode == 0x0D else (low - 0x10000 if low & 0x8000 else low))
                shifted_follow = word(shifted, shifted_start, next_address + shift)
                if PROGRAM[0] <= value < PROGRAM[1] and new == old and shifted_follow == follow:
                    found.append((address, f"lui/{opcode:02X} pair {value:08X} at {next_address:08X}"))
                break

    for address, what in found:
        print(f"{address:08X} {where(address):40} {what}")

    print(f"{len(found)} fixed addresses")


main()
