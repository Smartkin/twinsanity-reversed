#!/usr/bin/env python3
"""asmv.py FUNCTION...: a function's instructions with their labels, nops and the instructions splat writes as words"""
import re, sys
from pathlib import Path
D = Path(__file__).resolve().parent.parent / "asm" / "text"
for fn in sys.argv[1:]:
    print(f"== {fn}")
    for line in open(D / f"{fn}.s"):
        m = re.match(r"\s*/\* [0-9A-F]+ ([0-9A-F]{8}) [0-9A-F]+ \*/\s*(.*)", line)
        if m:
            text = m.group(2).strip()
            w = re.match(r"\.word\s+0x[0-9A-F]+\s+#\s+(\S+\s+[^#<]*?)\s*(?:#|$)", text)
            if w:
                text = w.group(1).strip() + "   (word)"
            print(f"  {m.group(1)[-5:]} {re.sub(r'\\s+', ' ', text)}")
            continue
        m = re.match(r"\s*(\.L[0-9A-F]+):", line)
        if m:
            print(f" {m.group(1)}:")
