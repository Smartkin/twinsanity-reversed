#!/usr/bin/env python3
"""heap_compare.py A B [case]: two folders' heap pool dumps (heap_dump.py: from 0x400000, where test runs put the pool in every
build) word by word, pointers into each build's static image compared as symbol + offset, the rest as they are; the differences
told by the heap block they're in (walked from the heap manager's first block: a 0x10 byte header, the size's bit 31 free) or the
small allocations' pages"""
import bisect, re, struct, sys
from pathlib import Path

def load_map(path):
    symbols = {}
    end = None
    for line in open(path, errors="replace"):
        m = re.match(r"\s+0x([0-9a-f]{8,16})\s+([A-Za-z_][\w.$]*)\s*$", line)
        if m:
            address = int(m.group(1), 16)
            name = m.group(2)
            if not name.endswith(".NON_MATCHING"):
                symbols.setdefault(address, name)
        m = re.match(r"\s+0x([0-9a-f]+)\s+_end = ALIGN", line)
        if m:
            end = int(m.group(1), 16)
    addresses = sorted(symbols)
    return addresses, symbols, end


BASE = 0x400000

class Build:
    def __init__(self, folder):
        self.addresses, self.symbols, self.end = load_map(folder / "SLES_525.68.map")

    def meaning(self, value):
        if 0x100000 <= value < BASE:
            i = bisect.bisect_right(self.addresses, value) - 1
            if i >= 0:
                return ("static", self.symbols[self.addresses[i]], value - self.addresses[i])
        return ("value", value)

def manager(folder, case):
    words = struct.unpack("<96I", (folder / f"heapmanager_{case}.bin").read_bytes())
    return {"first": words[1], "end": words[2], "poolStart": words[7], "poolSize": words[8], "small": words[0x16C // 4],
            "usedBlocks": words[4], "usedBytes": words[6]}

def blocks(data, first, end):
    result = []
    address = first
    while address + 0x10 <= end and address - BASE + 0x10 <= len(data):
        size_word = struct.unpack_from("<I", data, address - BASE + 0xC)[0]
        size = size_word & 0x7FFFFFFF
        if size == 0:
            break
        result.append((address + 0x10, size, size_word >> 31))
        address += 0x10 + size
    return result

if __name__ == "__main__":
    a, b = Path(sys.argv[1]), Path(sys.argv[2])
    case = sys.argv[3] if len(sys.argv) > 3 else "beach"
    ba, bb = Build(a), Build(b)
    ma, mb = manager(a, case), manager(b, case)
    if ma != mb:
        print(f"heap managers differ: {ma} vs {mb}")
    da, db = (a / f"heap_{case}.bin").read_bytes(), (b / f"heap_{case}.bin").read_bytes()
    walked = blocks(da, ma["first"], ma["end"])
    starts = [start for start, _, _ in walked]
    words = min(len(da), len(db)) // 4
    raw = 0
    real = []
    for i in range(words):
        x, y = struct.unpack_from("<I", da, 4 * i)[0], struct.unpack_from("<I", db, 4 * i)[0]
        if x != y:
            raw += 1
            if ba.meaning(x) != bb.meaning(y):
                real.append((BASE + 4 * i, x, y))
    groups = {}
    for address, x, y in real:
        if address >= ma["small"]:
            key = ("pages", (address - ma["small"]) & ~0xFFF, 0x1000, 0)
        else:
            k = bisect.bisect_right(starts, address) - 1
            key = ("block",) + walked[k] if k >= 0 and address < walked[k][0] + walked[k][1] else ("between", 0, 0, 0)
        groups.setdefault(key, []).append((address, x, y))
    print(f"heap {case}: {len(walked)} blocks walked, {words} words, {raw} differ raw, {len(real)} beyond the layout in "
          f"{len(groups)} blocks or pages ({sum(len(v) for k, v in groups.items() if k[0] == 'block' and k[3] == 0)} in used blocks)")
    for (kind, start, size, free), items in list(groups.items())[:24]:
        where = (f"block at {start:#x} ({size:#x} bytes, {'free' if free else 'used'})" if kind == "block" else
                 f"page at small+{start:#x}" if kind == "pages" else "outside the blocks")
        origin = start if kind == "block" else ma["small"] + start if kind == "pages" else 0
        print(f"  {where}: {len(items)} words" + "".join(
            f"\n     +{address - origin:#x}: {x:08x} {ba.meaning(x)} vs {y:08x} {bb.meaning(y)}" for address, x, y in items[:6]))
    print("heap dumps:", "same" if not real else f"{len(real)} words differ beyond the layout")
