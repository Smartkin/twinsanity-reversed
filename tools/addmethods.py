#!/usr/bin/env python3
"""addmethods.py FILE [conditions]: inserts declarations into include/game/commands.h's classes before each class's Size() (or
conditions.h's after each class's Destroy()). FILE has blocks 'ClassName' followed by indented declaration lines"""
import re, sys
from pathlib import Path
CONDITIONS = len(sys.argv) > 2 and sys.argv[2] == "conditions"
H = Path(__file__).resolve().parent.parent / "include" / "game" / ("conditions.h" if CONDITIONS else "commands.h")
text = H.read_text()
blocks = {}
current = None
for line in open(sys.argv[1]):
    if not line.strip():
        continue
    if not line.startswith(" "):
        current = line.strip()
        blocks[current] = []
    else:
        blocks[current].append(line.rstrip("\n"))
for name, lines in blocks.items():
    if CONDITIONS:
        m = re.search(rf"^class {name} : public ScriptCondition\n\{{\npublic:\n(.*?^    void Destroy\(u32 destroyFlags\)[^\n]*\n)", text, re.M | re.S)
        if not m:
            sys.exit(f"no class {name}")
        insert_at = m.end()
    else:
        m = re.search(rf"^class {name} : public ScriptCommand\n\{{\npublic:\n(.*?)^    u32 Size\(\)", text, re.M | re.S)
        if not m:
            sys.exit(f"no class {name}")
        insert_at = m.end() - len("    u32 Size()")
    existing = m.group(1)
    new = [l for l in lines if l.strip() not in existing]
    text = text[:insert_at] + "".join(l + "\n" for l in new) + text[insert_at:]
H.write_text(text)
print(len(blocks), "classes")
