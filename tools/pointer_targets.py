#!/usr/bin/env python3
"""Sorts the addresses tools/find_fixed_addresses.py lists (build/fixed.txt) into pointers and numbers.

A code address is a function's when the function before it ended within the 8 words before (jr ra: this compiler leaves a copy of
the delay slot's instruction and a nop after it) or it starts with a prologue (addiu sp, sp, -n). A data address is a pointer
when it's aligned and a known symbol starts there or right before. Everything else is a number that only looks like an address
(masks, sizes, packed shorts, ASCII).

    tools/pointer_targets.py [fixed.txt]
"""
import re
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
ELF = (HERE / "SLES_525.68").read_bytes()
TEXT = (0x100000, 0x2D9D88)
DATA = (0x2E6F00, 0x3DB200)


def word(address):
    return struct.unpack_from("<I", ELF, address - 0x100000 + 0x1000)[0]


def is_function_start(address):
    if address % 4 or not TEXT[0] <= address < TEXT[1]:
        return False

    if word(address) >> 16 == 0x27BD and word(address) & 0x8000:
        return True

    return any(word(address - 4 * i) == 0x03E00008 for i in range(2, 9) if address - 4 * i >= TEXT[0])


def classify(fixed_path):
    code, data, numbers = {}, {}, {}
    for line in open(fixed_path):
        match = re.match(r"([0-9A-F]{8}) (\S+)\s+(data word|lui/\w+ pair) ([0-9A-F]{8})", line)
        if not match:
            continue

        where, value = match.group(1), int(match.group(4), 16)
        if TEXT[0] <= value < TEXT[1] and is_function_start(value):
            code.setdefault(value, []).append(where)
        elif DATA[0] <= value < DATA[1] and value % 4 == 0 and match.group(3) == "data word":
            data.setdefault(value, []).append(where)
        else:
            numbers.setdefault(value, []).append(where)

    return code, data, numbers


def main():
    code, data, numbers = classify(sys.argv[1] if len(sys.argv) > 1 else HERE / "build" / "fixed.txt")
    print(f"code pointers {len(code)}, data pointers {len(data)}, numbers {len(numbers)}")
    for title, table in (("data", data), ("numbers", numbers)):
        print(f"-- {title}")
        for value in sorted(table):
            print(f"  {value:08X} from {' '.join(table[value][:3])}")


if __name__ == "__main__":
    main()
