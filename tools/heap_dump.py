#!/usr/bin/env python3
"""heap_dump.py FOLDER CASE: a render check case on FOLDER's ELF with its heap pool and the heap manager dumped when it freezes
(test runs put the pool at 0x400000 whatever the build, platform/ps2/memory.cpp)"""
import shutil, subprocess, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "tools"))
import render_check as rc
START = 0x400000
SIZE = 0xB60000
folder = Path(sys.argv[1])
case = sys.argv[2] if len(sys.argv) > 2 else "beach"
copy, map_copy = rc.test_copy(folder / "SLES_525.68.elf", folder / "SLES_525.68.map", folder)
inputs, freeze = rc.CASES[case]
card = folder / "run_card.ps2"
shutil.copy2(rc.CARD, card)
arguments = [sys.executable, str(HERE / "tools" / "run_pcsx2.py"), str(copy), rc.ISO, "--map", str(map_copy), "--manual",
             "--timeout", "400", "--snapshots", str(folder), "--card", str(card), "--at-frame", f"{freeze}:freeze",
             "--freeze-range", f"{START:x}:{SIZE}:{folder / ('heap_' + case + '.bin')}",
             "--freeze-dump", f"D_0031B338:96:{folder / ('heapmanager_' + case + '.bin')}"]
for item in inputs:
    arguments += ["--at-frame", item]
output = subprocess.run(arguments, cwd=HERE, capture_output=True, text=True).stdout
print("\n".join(line for line in output.splitlines() if "RESULT" in line or "rror" in line))
