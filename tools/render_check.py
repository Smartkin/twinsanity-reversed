#!/usr/bin/env python3
"""Compares what a build draws with what an earlier build drew, frame for frame: each case runs the game in PCSX2 with inputs at
exact frames and stops it after a frame (tools/run_pcsx2.py --at-frame, include/debug.h), whose picture has to come out byte for
byte the same as the reference's. The runs use test time (g_DebugFixedTime, set in a copy of the ELF): every frame takes the
frame's time, a frame's background work a set number of steps and each read is waited for, so frames don't move with how
fast the code is. The intro
movie's length in frames isn't steady, so the gameplay cases start playing the way TT Lab's Play does, past the movie. Every run
plays with a fresh copy of one card image (build/render_reference/card.ps2, a copy of the user's slot 1 card), so the menus that
read the card show the same saves.

    tools/render_check.py [--elf ELF --map MAP] reference [case...]   the build's pictures become the references
                                                                       (build/render_reference/)
    tools/render_check.py [--elf ELF --map MAP] [case...]              the build's pictures against them (build/render_check/)
"""
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import local_config

HERE = Path(__file__).resolve().parent.parent
ISO = local_config.disc_image()
# The title comes at frame 1208 in test time
NEW_GAME = ["1277:state=10", "1327:state=11"]
# Name: the inputs and the frame to stop after
CASES = {
    "logo_movie": ([], 350),
    "title_cutscene": ([], 1877),
    "main_menu": (["1277:press=start:5"], 1397),
    "beach": (NEW_GAME, 1777),
    "beach_running": (NEW_GAME + ["1477:press=up+right:150", "1577:press=cross:5"], 1592),
    "options_menu": (["1277:press=start:5", "1477:press=down:5", "1527:press=down:5", "1577:press=cross:5"], 1727),
    "pause_menu": (NEW_GAME + ["1577:press=start:5"], 1657),
    "load_menu": (["1277:press=start:5", "1477:press=down:5", "1527:press=cross:5"], 1727),
    # The fourth save loaded (the cave tunnel), unpaused and left idle about 21 seconds
    "cave": (["1277:press=start:5", "1477:press=down:5", "1527:press=cross:5", "1777:press=up:5", "1827:press=up:5",
              "1877:press=cross:5", "2400:press=cross:5"], 3700),
}
CARD = HERE / "build" / "render_reference" / "card.ps2"


def symbol(map_path, name):
    for line in open(map_path):
        match = re.match(r"\s+0x([0-9a-f]{8,16})\s+(\w+)\s*$", line)
        if match and match.group(2) == name:
            return int(match.group(1), 16)
    raise SystemExit(f"{map_path} has no {name}")


def test_copy(elf, map_path, folder):
    """A copy of the ELF with g_DebugFixedTime set in its .data, and of its map (the build can change while the cases run)"""
    data = bytearray(Path(elf).read_bytes())
    address = symbol(map_path, "g_DebugFixedTime")
    phoff, phnum = struct.unpack_from("<I", data, 0x1C)[0], struct.unpack_from("<H", data, 0x2C)[0]
    for index in range(phnum):
        kind, offset, vaddr, _, size = struct.unpack_from("<IIIII", data, phoff + 32 * index)
        if kind == 1 and vaddr <= address < vaddr + size:
            struct.pack_into("<I", data, offset + address - vaddr, 1)
            break
    else:
        raise SystemExit(f"g_DebugFixedTime ({address:#x}) isn't in the ELF's file")
    copy = folder / "test.elf"
    copy.write_bytes(data)
    map_copy = folder / "test.map"
    shutil.copy2(map_path, map_copy)
    return copy, map_copy


def run(case, elf, map_path, folder):
    inputs, freeze = CASES[case]
    card = folder / "run_card.ps2"
    shutil.copy2(CARD, card)
    arguments = [sys.executable, str(HERE / "tools" / "run_pcsx2.py"), str(elf), ISO, "--map", str(map_path), "--manual",
                 "--timeout", "200", "--snapshots", str(folder), "--card", str(card), "--at-frame", f"{freeze}:freeze"]
    for item in inputs:
        arguments += ["--at-frame", item]
    picture = folder / f"frame_{freeze}.png"
    picture.unlink(missing_ok=True)
    output = subprocess.run(arguments, cwd=HERE, capture_output=True, text=True).stdout
    if not picture.exists():
        print(f"{case}: no picture\n{output}")
        return None
    target = folder / f"{case}.png"
    picture.replace(target)
    return target


def main():
    if not ISO or not Path(ISO).exists():
        raise SystemExit("The PAL disc image isn't set or isn't there: put its path in local.json (\"disc_image\") or "
                         "$TWINSANITY_ISO")

    arguments = sys.argv[1:]
    elf, map_path = HERE / "build" / "SLES_525.68.elf", HERE / "build" / "SLES_525.68.map"
    while arguments[:1] and arguments[0] in ("--elf", "--map"):
        if arguments[0] == "--elf":
            elf = Path(arguments[1])
        else:
            map_path = Path(arguments[1])
        arguments = arguments[2:]
    reference = arguments[:1] == ["reference"]
    cases = arguments[reference:] or list(CASES)
    folder = HERE / "build" / ("render_reference" if reference else "render_check")
    folder.mkdir(parents=True, exist_ok=True)
    copy, map_copy = test_copy(elf, map_path, folder)
    if reference:
        shutil.copy2(elf, folder / "reference.elf")
        shutil.copy2(map_path, folder / "reference.map")

    failed = 0
    for case in cases:
        picture = run(case, copy, map_copy, folder)
        if reference or picture is None:
            failed += picture is None
            print(f"{case}: {'kept' if picture else 'FAILED'}", flush=True)
            continue
        expected = HERE / "build" / "render_reference" / f"{case}.png"
        same = expected.exists() and expected.read_bytes() == picture.read_bytes()
        failed += not same
        print(f"{case}: {'same' if same else 'DIFFERENT' if expected.exists() else 'no reference'}", flush=True)

    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
