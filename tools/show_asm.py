#!/usr/bin/env python3
"""Prints functions' asm compactly (address, instruction, no encodings) for reading.

    tools/show_asm.py <function>...
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent

for name in sys.argv[1:]:
    path = HERE / "asm" / "text" / f"{name}.s"
    print(f"== {name}")
    for line in path.read_text().splitlines():
        match = re.match(r"\s*/\* [0-9A-F]+ ([0-9A-F]{8}) [0-9A-F]{8} \*/\s+(.*)", line)
        if match:
            print(f"  {match.group(1)[2:]}  {re.sub(r'\s+', ' ', match.group(2))}")
        elif re.match(r"\s*\.L[0-9A-F]+:|\s*[gj]label", line):
            print(line.strip().replace("glabel ", "").replace("jlabel ", ""))
