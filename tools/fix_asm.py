#!/usr/bin/env python3
"""Fixes what splat's output gets wrong about the layout, run after every split.

The crt0 and the thread setup load $gp with the address 0x311870 (.sdata + 0x7FF0), which spimdisasm names after the .bss
variable that happens to be there; they get the linker's _gp, which follows .sdata wherever it goes. The end of .bss (0x3DB200)
is the end of its last array to FUN_002941b0 (G_BLEND_SHAPE_FLOATS_end) but the linker's _end to the retail entry, which clears
.bss up to it, and to sbrk, whose first break it is: those two get _end, which follows what's linked after the retail .bss.
libmpeg's mask in .text (D_002BF5B0) is read with lq, which needs it 16 byte aligned: the asm only aligns to 8, and code
replaced before it moved it half a quadword (the movies came out striped). The default case of the dead switch
func_001412B0 (.L0014130C, in its case stubs' file) is a glabel, which keeps the link from leaving the stubs' asm out once
they're C++: it becomes a jump label, which the link script defines as 0 when its file is left out. splat includes binary data by its absolute path,
which becomes one relative to this folder (ninja runs the assembler here), so the tree can be moved or copied to another system.

    tools/fix_asm.py
"""
import re
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
RENAMES = {"D_00311870": "_gp"}
# asm file: (what, what instead), once
ALIGNMENTS = {"text/D_002BF5B0.s": (".align 3\nnonmatching D_002BF5B0", ".align 4\nnonmatching D_002BF5B0")}
# asm file: (what, what instead), once: labels of a dead function's branches that splat made global
LABELS = {"text/FUN_001412fc.s": ("glabel .L0014130C", "jlabel .L0014130C")}
# asm file: {symbol: symbol}, for addresses that mean something else in one place
FILE_RENAMES = {
    "text/entry.s": {"G_BLEND_SHAPE_FLOATS_end": "_end"},
    "data/data.data.s": {"G_BLEND_SHAPE_FLOATS_end": "_end"},
}


def relative_incbin(match):
    path = Path(match.group(1))
    if not path.is_absolute():
        return match.group(0)

    try:
        return f'.incbin "{path.resolve().relative_to(HERE).as_posix()}"'
    except ValueError:
        raise SystemExit(f"fix_asm: {path} is outside {HERE}")


def main():
    changed = 0
    for path in (HERE / "asm").rglob("*.s"):
        text = path.read_text()
        if ".incbin" not in text:
            continue

        fixed = re.sub(r'\.incbin "([^"]+)"', relative_incbin, text)
        if fixed != text:
            path.write_text(fixed, newline="\n")
            changed += 1

    for path in (HERE / "asm" / "text").glob("*.s"):
        text = path.read_text()
        fixed = text
        for old, new in RENAMES.items():
            fixed = fixed.replace(f"%hi({old})", f"%hi({new})").replace(f"%lo({old})", f"%lo({new})")

        if fixed != text:
            path.write_text(fixed, newline="\n")
            changed += 1

    for name, renames in FILE_RENAMES.items():
        path = HERE / "asm" / name
        text = path.read_text()
        fixed = text
        for old, new in renames.items():
            fixed = fixed.replace(f"%hi({old})", f"%hi({new})").replace(f"%lo({old})", f"%lo({new})")
            fixed = fixed.replace(f".word {old}\n", f".word {new}\n")

        if fixed == text and not all(new in text for new in renames.values()):
            raise SystemExit(f"fix_asm: nothing to rename in {name}")

        if fixed != text:
            path.write_text(fixed, newline="\n")
            changed += 1

    for name, (old, new) in list(ALIGNMENTS.items()) + list(LABELS.items()):
        path = HERE / "asm" / name
        text = path.read_text()
        if old in text:
            path.write_text(text.replace(old, new, 1), newline="\n")
            changed += 1
        elif new not in text:
            raise SystemExit(f"fix_asm: no {old!r} in {name}")

    print(f"fix_asm: {changed} files")


main()
