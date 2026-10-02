#!/usr/bin/env python3
"""Boots a built ELF in PCSX2 (its Flatpak) with a disc image of the game and checks that it gets to play the beach.

The game runs as it does for a player: the logo movies, the loading, the title's "THREE YEARS AGO..." cutscene, then "PRESS START
BUTTON", where the run starts a new game (the only input it gives, through the game controller's next state, G_GameController + 8
bits 50-55, the way the main menu's New Game does), the intro movie and the level. It's all driven through PINE memory writes, no
input devices. --quick goes the way TT Lab's Play does instead: past the logos, the cutscene and the intro movie straight to
playing. PINE is turned on for the ELF's CRC alone (PCSX2 names an overridden executable's game settings after it), on a slot of
its own, and whatever the run put into PCSX2's config goes again afterwards.

    tools/run_pcsx2.py <elf> <disc image> [--map build/SLES_525.68.map] [--timeout 240] [--quick] [--title-wait 16] [--stay]
                       [--watch symbol[+offset][>offset]...]... [--snapshot T]... [--pad] [--manual] [--press T:BUTTONS[:SECONDS]]...
                       [--goto T:STATE]... [--card FILE] [--chunks] [--loaders] [--string SYMBOL>OFFSET]...
                       [--keep-state T:FILE]... [--backtrace T]... [--monitor NAME]

--watch prints a word of the map's symbol whenever it changes (a breadcrumb the code under test writes, say; every >offset
follows the pointer found so far: G_GameReadersStorages>0x18>0x4 is storage 0's current reader's vtable). --snapshot T saves
a state T seconds after the start (PINE, slot 9) and keeps the screenshot PCSX2 puts in it (in --snapshots, default
build/snapshots) and deletes the state, never for the retail executable's CRC, whose states are the user's. --keep-state T:FILE
keeps a copy of such a state as FILE (the EE's registers are in its "PCSX2 Internal Structures.dat", 32 GPRs of 16 bytes found by
$gp's value, then HI, LO and the COP0 registers). --pad prints pad 1's
buttons whenever they change. The game gets a copy of the user's slot 1 memory card (slot 2 empty), deleted afterwards: what it
writes never reaches the user's cards. --press holds buttons of pad 1 (names joined by +: start, select, up, down, left, right,
triangle, circle, cross, square, l1, l2, r1, r2) T seconds after the start for SECONDS (0.2 by default), through the PS2 pad
layer's g_TestPadButtons (src/platform/ps2/pads.cpp), no input device involved. --manual drives nothing: the game goes through its
menus by itself (and --press, --goto: the game controller's next state at T) until --timeout, the menus' saves included. --card
plays with that card image in slot 1 instead of a copy of the user's (copied in, and back once PCSX2 is gone, so the next run sees
what this one saved; never one of the user's cards in PCSX2's folder). --chunks prints the chunk manager's chunks and --loaders
the chunk loading manager's loaders (bits, the RM2 and SM2 loaders' states, the SM2's data's state and the parts it still has)
whenever they change; --string prints a game String past a pointer. --backtrace T prints the code addresses on the main thread's
stack in the scratchpad that follow a jal or jalr (live frames and stale ones alike). A hang that isn't a loop of the game's often
shows in PCSX2's log (emulog.txt in its config's logs): TLB misses at low addresses are null pointers, "# Syscall: undefined"
is a jump into nowhere that the BIOS stopped at. On KDE Plasma, PCSX2's window goes to one monitor (--monitor, $PCSX2_MONITOR,
HDMI-A-1 by default; kscreen-doctor -o names them), below other windows and without taking the focus (tools/kwin.py).
"""
import zipfile
import argparse
import bisect
import contextlib
import os
import re
import shutil
import signal
import struct
import zlib
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pine import Pine
import kwin

CONFIG = os.path.expanduser("~/.var/app/net.pcsx2.PCSX2/config/PCSX2")
SLOT = 28112
RETAIL_CRC = 0x1510E1D1
SNAPSHOT_SLOT = 9
RUN_CARD = "TwinsanityDecompRun.ps2"
LOGOS, LOAD_START_CHUNK, TITLE, NEW_GAME, START_PLAYING, PLAYING, MOVIE = 4, 6, 7, 10, 11, 12, 14
# The report's button word as the game reads it (src/platform/ps2/pads.cpp)
BUTTONS = {"select": 0x100, "l3": 0x200, "r3": 0x400, "start": 0x800, "up": 0x1000, "right": 0x2000, "down": 0x4000,
           "left": 0x8000, "l2": 0x1, "r2": 0x2, "l1": 0x4, "r1": 0x8, "triangle": 0x10, "circle": 0x20, "cross": 0x40,
           "square": 0x80}
NEXT = {LOGOS: LOAD_START_CHUNK, TITLE: NEW_GAME, MOVIE: START_PLAYING}


def crc(path):
    data = open(path, "rb").read()
    value = 0
    for (word,) in struct.iter_unpack("<I", data[:len(data) // 4 * 4]):
        value ^= word
    return value


def map_symbols(path, names):
    found = {}
    for line in open(path):
        match = re.match(r"\s+0x([0-9a-f]{8,16})\s+(\w+)\s*$", line)
        if match and match.group(2) in names:
            found[match.group(2)] = int(match.group(1), 16)

    missing = set(names) - set(found)
    if missing:
        raise SystemExit(f"{path} has no {', '.join(sorted(missing))}")

    return found


@contextlib.contextmanager
def saved_state(pine, elf_crc, when):
    """Saves a state (PCSX2 names it after the CRC and the slot) and yields its path, deleting it afterwards (None when PCSX2
    saved none). Never for the retail executable, whose states are the user's"""
    if elf_crc == RETAIL_CRC:
        raise SystemExit("No save states of the retail executable: its states are the user's")
    states = f"{CONFIG}/sstates"
    before = set(os.listdir(states)) if os.path.isdir(states) else set()
    pine.save_state(SNAPSHOT_SLOT)
    for _ in range(100):
        time.sleep(0.1)
        made = [name for name in (set(os.listdir(states)) - before) if f"{elf_crc:08X}" in name and name.endswith(f".{SNAPSHOT_SLOT:02d}.p2s")]
        if made:
            break
    else:
        print(f"{when:6.1f}s no state saved", flush=True)
        yield None
        return

    time.sleep(1)
    try:
        yield f"{states}/{made[0]}"
    finally:
        for name in set(os.listdir(states)) - before:
            if f"{elf_crc:08X}" in name:
                os.remove(f"{states}/{name}")


def take_snapshot(pine, elf_crc, when, folder, name=None):
    # The state keeps a screenshot
    with saved_state(pine, elf_crc, when) as path:
        if path is None:
            return
        os.makedirs(folder, exist_ok=True)
        out_path = f"{folder}/{name or f'snapshot_{when:05.1f}s'}.png"
        with zipfile.ZipFile(path) as state:
            pictures = [entry for entry in state.namelist() if entry.lower().endswith(".png")]
            for entry in pictures:
                with open(out_path, "wb") as out:
                    out.write(state.read(entry))
        print(f"{when:6.1f}s snapshot {out_path if pictures else 'without a picture'}", flush=True)


def descendant(ancestor):
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue

        try:
            if not open(f"/proc/{entry}/comm").read().startswith("pcsx2"):
                continue

            pid = int(entry)
            while pid > 1:
                if pid == ancestor:
                    return int(entry)

                stat = open(f"/proc/{pid}/stat").read()
                pid = int(stat[stat.rindex(")") + 2:].split()[1])
        except (OSError, ValueError):
            pass

    return None


def check_disk_manager(pine, manager):
    """Walks the disk manager's nodes (game/disk.h's layout) and checks what they have to agree on. Prints a summary that doesn't
    depend on where the compaction left things: the used blocks' sizes and the free bytes"""
    pool, pool_size = pine.read32(manager), pine.read32(manager + 4)
    free_lists = [pine.read32(manager + 0xC + 4 * i) for i in range(16)]
    free_count, node = pine.read32(manager + 0x4C), pine.read32(manager + 0x50)
    handles_used = pine.read32(manager + 0x8140)
    problems, used_sizes, free_bytes, free_nodes, at, previous, count = [], [], 0, set(), pool, 0, 0
    while node and count < 20000:
        index, memory, size = (pine.read32(node + 4 * i) for i in range(3))
        back, bits = pine.read32(node + 0x14), pine.read32(node + 0x1C)
        state = bits & 0xF
        if memory != at:
            problems.append(f"node {node:#x} at {memory:#x}, expected {at:#x}")
        if back != previous:
            problems.append(f"node {node:#x} links back to {back:#x}, not {previous:#x}")
        if state == 1:
            free_bytes += size
            free_nodes.add(node)
        elif state == 2:
            used_sizes.append(size)
            if pine.read32(manager + 0x58 + 4 * index) != node:
                problems.append(f"node {node:#x}'s handle {index} points elsewhere")
        else:
            problems.append(f"node {node:#x} in state {state}")
        at, previous, node, count = memory + size, node, pine.read32(node + 0x18), count + 1
    if at != pool + pool_size:
        problems.append(f"the nodes end at {at:#x}, the pool at {pool + pool_size:#x}")
    listed = 0
    for size_class, free in enumerate(free_lists):
        last = 0
        while free:
            listed += 1
            size = pine.read32(free + 8)
            if free not in free_nodes:
                problems.append(f"free list {size_class} has {free:#x}, which isn't a free node")
            if size < last:
                problems.append(f"free list {size_class} isn't by size")
            last, free = size, pine.read32(free + 0x10)
    if listed != free_count or listed != len(free_nodes):
        problems.append(f"{listed} nodes in the free lists, {len(free_nodes)} free nodes, a count of {free_count}")
    if len(used_sizes) != handles_used:
        problems.append(f"{len(used_sizes)} used nodes, {handles_used} handles")
    digest = zlib.crc32(struct.pack(f"<{len(used_sizes)}I", *sorted(used_sizes)))
    print(f"disk manager: {count} nodes, {len(used_sizes)} used ({sum(used_sizes):#x} bytes, sizes digest {digest:08x}), "
          f"{free_bytes:#x} bytes free in {len(free_nodes)} nodes")
    for problem in problems[:20]:
        print("  disk manager problem:", problem)


PARTS = [("scenery", 0x144), ("clocks", 0x154), ("instances", 0x15C), ("collision", 0x1BC), ("lights", 0x1C0),
         ("1dc", 0x1DC), ("particles", 0x1E0)]


def backtrace(pine, elf, map_path):
    """The return addresses on the main thread's stack (in the scratchpad), innermost first: the words of the stack that point
    just past a jal or jalr of the executable, with the function they're in"""
    with open(elf, "rb") as file:
        data = file.read()
    phoff, phnum = struct.unpack_from("<I", data, 0x1C)[0], struct.unpack_from("<H", data, 0x2C)[0]
    segments = [struct.unpack_from("<IIIII", data, phoff + 32 * i) for i in range(phnum)]

    def instruction(address):
        for kind, offset, vaddr, _, size in segments:
            if kind == 1 and vaddr <= address < vaddr + size:
                return struct.unpack_from("<I", data, offset + address - vaddr)[0]
        return None

    functions = []
    for line in open(map_path):
        match = re.match(r"\s+0x([0-9a-f]{8,16})\s+(\S+)\s*$", line)
        if match and not match.group(2).endswith(".NON_MATCHING"):
            functions.append((int(match.group(1), 16), match.group(2)))
    functions.sort()
    starts = [address for address, _ in functions]
    found = []
    for address in range(0x70000000, 0x70004000, 4):
        word = pine.read32(address)
        call = instruction(word - 8) if word & 3 == 0 and 0x100000 <= word < 0x2E0000 else None
        if call is None or not (call >> 26 == 3 or (call >> 26 == 0 and call & 0x3F == 9)):
            continue
        index = bisect.bisect_right(starts, word) - 1
        found.append(f"  {address:#x}: {word:#x} {functions[index][1]}+{word - functions[index][0]:#x}")
    return found


def describe_loaders(pine, symbol):
    """game/chunkloading.h: the manager's bits, then each loader's path, bits and its RM2 and SM2 loaders' states"""
    manager = pine.read32(symbol)
    if not manager:
        return None
    loaders = []
    loader = pine.read32(manager + 0x18)
    while loader and len(loaders) < 16:
        rm2, sm2 = pine.read32(loader + 0x20), pine.read32(loader + 0x24)
        states = "/".join(str(pine.read32(part + 0x10)) if part else "-" for part in (rm2, sm2))
        description = f"{pine.string(loader + 0xC)} {pine.read32(loader + 4):#x} {states}"
        reference = pine.read32(sm2 + 0x1C) if sm2 else 0
        chunk = pine.read32(reference) if reference else 0
        if chunk:
            # game/chunkdata.h: the state (bits 18-21, bit 24 released in steps), the RM2 loads and the parts still there
            bits = pine.read32(chunk + 0x120)
            parts = [name for name, offset in PARTS if pine.read32(chunk + offset) not in (0, 0xFFFFFFFF)]
            description += (f" [state {bits >> 18 & 0xF}{' steps' if bits & 0x1000000 else ''} rm2 {pine.read32(chunk + 0x124)}"
                            f" {','.join(parts)}]")
        loaders.append(description)
        loader = pine.read32(loader + 0x2C)
    return f"{pine.read32(manager):#x} " + "; ".join(loaders)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("elf")
    parser.add_argument("iso")
    parser.add_argument("--map", default="build/SLES_525.68.map")
    parser.add_argument("--timeout", type=float, default=240)
    parser.add_argument("--stay", action="store_true", help="leave PCSX2 running once the game plays")
    parser.add_argument("--watch", action="append", default=[], help="a symbol whose word to print when it changes")
    parser.add_argument("--quick", action="store_true", help="skip the logos, the title's cutscene and the intro movie")
    parser.add_argument("--title-wait", type=float, default=16,
                        help="seconds at the title before the new game starts: its cutscene plays first")
    parser.add_argument("--pad", action="store_true", help="print pad 1's buttons whenever they change")
    parser.add_argument("--check-disk", action="store_true", help="check the disk manager's nodes once the game plays")
    parser.add_argument("--dump", action="append", default=[], metavar="SYMBOL[+OFFSET]:WORDS",
                        help="print that many words from the map's symbol when the title comes up and once the game plays")
    parser.add_argument("--snapshot", action="append", type=float, default=[], help="seconds after the start to take a picture at")
    parser.add_argument("--snapshots", default="build/snapshots", help="where the pictures go")
    parser.add_argument("--manual", action="store_true", help="drive nothing, run until --timeout")
    parser.add_argument("--card", help="a card image (not one of PCSX2's own) to play with in slot 1, written back after")
    parser.add_argument("--chunks", action="store_true", help="print the chunk manager's chunks whenever they change")
    parser.add_argument("--backtrace", action="append", default=[], type=float, metavar="T",
                        help="print the return addresses on the main thread's stack T seconds after the start")
    parser.add_argument("--keep-state", action="append", default=[], metavar="T:FILE",
                        help="save a state T seconds after the start and keep a copy of it as FILE (not for the retail executable)")
    parser.add_argument("--at-frame", action="append", default=[], metavar="N:WHAT",
                        help="at the end of frame N (debug.h's frame count): state=S asks for the game controller state S, "
                             "press=BUTTONS[:FRAMES] holds pad 1's buttons (10 frames by default), freeze stops the game after the "
                             "frame, keeps a snapshot of it (frame_N.png) and ends the run")
    parser.add_argument("--freeze-dump", action="append", default=[], metavar="SYMBOL:WORDS:FILE",
                        help="at an --at-frame freeze, the words at the symbol into the file")
    parser.add_argument("--freeze-range", action="append", default=[], metavar="ADDRESS:BYTES:FILE",
                        help="at an --at-frame freeze, the bytes of EE memory from the address (hex, 8-byte aligned) into the file")
    parser.add_argument("--loaders", action="store_true",
                        help="print the chunk loading manager's loaders (path, bits, RM2 and SM2 states) whenever they change")
    parser.add_argument("--goto", action="append", default=[], metavar="T:STATE",
                        help="ask the game controller for the state T seconds after the start (with --manual)")
    parser.add_argument("--string", action="append", default=[], metavar="SYMBOL>OFFSET",
                        help="print the game String at the offset past the pointer the symbol holds whenever it changes")
    parser.add_argument("--press", action="append", default=[], metavar="T:BUTTONS[:SECONDS]",
                        help="hold pad 1's buttons T seconds after the start")
    parser.add_argument("--monitor", help="the monitor PCSX2's window goes to, below other windows and without the focus (KWin's "
                                          f"output name; default $PCSX2_MONITOR or {kwin.DEFAULT_MONITOR}, empty for anywhere)")
    args = parser.parse_args()

    # debug.h's DebugInput table: frame, kind (1 a state, 2 buttons), value, length
    frame_inputs, freeze_frame = [], None
    for part in args.at_frame:
        frame, _, what = part.partition(":")
        kind, _, value = what.partition("=")
        if kind == "freeze":
            freeze_frame = int(frame, 0)
        elif kind == "state":
            frame_inputs.append((int(frame, 0), 1, int(value, 0), 0))
        elif kind == "press":
            names, _, length = value.partition(":")
            mask = 0
            for name in names.split("+"):
                mask |= BUTTONS[name.lower()]
            frame_inputs.append((int(frame, 0), 2, mask, int(length or "10", 0)))
        else:
            raise SystemExit(f"--at-frame {part}: state=, press= or freeze")
    if len(frame_inputs) > 16:
        raise SystemExit("--at-frame: 16 inputs at most")

    presses = []
    for part in args.press:
        when, names, *hold = part.split(":")
        mask = 0
        for name in names.split("+"):
            mask |= BUTTONS[name.lower()]
        presses.append([float(when), float(hold[0]) if hold else 0.2, mask, None])

    # "name+offset" watches a word past a symbol, "name>offset" a word past the pointer the symbol holds, and every further
    # ">offset" follows the pointer found ("name+offset>offset" starts from the word past the symbol)
    watches = []
    for part in args.watch:
        where, *hops = part.split(">")
        symbol, _, at = where.partition("+")
        watches.append((part, symbol, int(at or "0", 0), [int(hop or "0", 0) for hop in hops]))
    # "symbol[+offset]:words"
    dumps = []
    for part in args.dump:
        where, count = part.rsplit(":", 1)
        symbol, offset = (where.split("+")[0], int(where.split("+")[1], 0)) if "+" in where else (where, 0)
        dumps.append((where, symbol, offset, int(count, 0)))
    symbols = map_symbols(args.map, {"G_GameController", "G_ChunkManager", "G_GamePadController", "D_0031B4B8",
                                     *(["G_ChunkLoadingManager_"] if args.loaders else []),
                                     *(["g_TestPadButtons"] if presses else []),
                                     *(["g_DebugFrame", "g_DebugFreezeFrame", "g_DebugInputs", "g_DebugStates"] if args.at_frame else []),
                                     *(part.partition(">")[0] for part in args.string),
                                     *(symbol for _, symbol, _, _ in watches),
                                     *(symbol for _, symbol, _, _ in dumps),
                                     *(part.split(":")[0].lstrip("*").split("+")[0] for part in args.freeze_dump)})
    snapshots = sorted(args.snapshot)
    if snapshots and crc(args.elf) == RETAIL_CRC:
        raise SystemExit("No snapshots of the retail executable: its save states are the user's")
    watched = {}
    elf = os.path.abspath(args.elf)
    iso = os.path.abspath(args.iso)
    settings = f"{CONFIG}/gamesettings/{crc(elf):08X}.ini"
    backup = settings + ".before_decomp_run"
    had_settings = os.path.exists(settings)
    if had_settings:
        shutil.copy2(settings, backup)

    os.makedirs(os.path.dirname(settings), exist_ok=True)
    cards = f"{CONFIG}/memcards"
    user_cards = {name: os.path.getmtime(f"{cards}/{name}") for name in os.listdir(cards) if name != RUN_CARD}
    if args.card and os.path.realpath(os.path.dirname(os.path.abspath(args.card))) == os.path.realpath(cards):
        raise SystemExit("--card has to be a copy outside PCSX2's memory card folder")
    shutil.copy2(args.card if args.card else f"{cards}/Mcd001.ps2", f"{cards}/{RUN_CARD}")
    with open(settings, "w") as file:
        file.write(f"[EmuCore]\nEnablePINE = true\nPINESlot = {SLOT}\n\n"
                   f"[MemoryCards]\nSlot1_Enable = true\nSlot1_Filename = {RUN_CARD}\nSlot2_Enable = false\n")

    placement = kwin.windows_on(kwin.monitor_from(args.monitor))
    placement.__enter__()
    launcher = subprocess.Popen(["flatpak", "run", f"--filesystem={os.path.dirname(elf)}:ro", f"--filesystem={os.path.dirname(iso)}:ro",
                                 "net.pcsx2.PCSX2", "-nogui", "-fastboot", "-elf", elf, "--", iso],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    start = time.time()
    seen = []
    result = "TIMEOUT"
    try:
        child = None
        while child is None and time.time() - start < 60:
            time.sleep(0.5)
            child = descendant(launcher.pid)

        if child is None:
            raise SystemExit("PCSX2 didn't start")

        pine = None
        while pine is None and time.time() - start < 90:
            try:
                pine = Pine(f"/proc/{child}/root/run/user/{os.getuid()}/pcsx2.sock.{SLOT}")
                pine.read32(symbols["G_GameController"])
            except Exception:
                pine = None
                time.sleep(0.5)

        if pine is None:
            raise SystemExit("PCSX2 didn't open PINE")

        # The frame inputs go in once the game has made its controller: earlier, the start-up's clearing of .bss (or the ELF's
        # loading) could undo them
        inputs_written = not args.at_frame
        states_log = []

        playing_since = None
        title_since = None
        buttons = None
        held = 0
        chunks_seen = None
        loaders_seen = None
        gotos = sorted((float(part.split(":")[0]), int(part.split(":")[1], 0)) for part in args.goto)
        while time.time() - start < args.timeout:
            try:
                controller = pine.read32(symbols["G_GameController"])
                state = pine.read32(controller + 0xC) >> 12 & 0x3F if controller else 0
            except Exception:
                time.sleep(0.2)
                continue

            if not inputs_written and controller:
                for index, (frame, kind, value, length) in enumerate(frame_inputs):
                    for offset, word in enumerate((frame, kind, value, length)):
                        pine.write32(symbols["g_DebugInputs"] + 16 * index + 4 * offset, word)
                if freeze_frame is not None:
                    pine.write32(symbols["g_DebugFreezeFrame"], freeze_frame)
                written_at = pine.read32(symbols["g_DebugFrame"])
                if any(frame <= written_at for frame, *_ in frame_inputs) or (freeze_frame or written_at + 1) <= written_at:
                    print(f"WARNING: --at-frame written at frame {written_at}, after a frame it names", flush=True)
                inputs_written = True

            if snapshots and time.time() - start >= snapshots[0]:
                take_snapshot(pine, crc(args.elf), snapshots.pop(0), args.snapshots)

            if presses:
                # A press starts at the first look past its time (a snapshot can take a second) and lasts its time from there
                now = time.time() - start
                mask = 0
                for press in presses:
                    when, hold, buttons_held, began = press
                    if began is None and now >= when:
                        press[3] = began = now
                    if began is not None and now < began + hold:
                        mask |= buttons_held
                if mask != held:
                    held = mask
                    pine.write32(symbols["g_TestPadButtons"], mask)
                    print(f"{now:6.1f}s holding {hex(mask)}", flush=True)

            for name, symbol, at, hops in watches:
                value = pine.read32(symbols[symbol] + at)
                for hop in hops:
                    value = pine.read32(value + hop) if value else None
                if watched.get(name) != value:
                    watched[name] = value
                    print(f"{time.time() - start:6.1f}s {name} = {value if value is None else hex(value)}", flush=True)

            if args.chunks:
                manager = pine.read32(symbols["G_ChunkManager"])
                loaded = None
                if manager:
                    count = pine.read32(manager) & 0xFFFF
                    loaded = [pine.string(pine.read32(manager + 4 + 4 * i)) for i in range(min(count, 8))]
                if loaded != chunks_seen:
                    chunks_seen = loaded
                    print(f"{time.time() - start:6.1f}s chunks {loaded}", flush=True)

            if args.keep_state and time.time() - start >= float(args.keep_state[0].split(":", 1)[0]):
                copy = args.keep_state.pop(0).split(":", 1)[1]
                with saved_state(pine, crc(args.elf), time.time() - start) as path:
                    if path is not None:
                        shutil.copy2(path, copy)
                        print(f"{time.time() - start:6.1f}s state kept as {copy}", flush=True)

            if args.backtrace and time.time() - start >= args.backtrace[0]:
                print(f"{time.time() - start:6.1f}s main thread's stack:", flush=True)
                for line in backtrace(pine, args.elf, args.map):
                    print(line, flush=True)
                args.backtrace.pop(0)

            if args.loaders:
                loaders = describe_loaders(pine, symbols["G_ChunkLoadingManager_"])
                if loaders != loaders_seen:
                    loaders_seen = loaders
                    print(f"{time.time() - start:6.1f}s loaders {loaders}", flush=True)

            for part in args.string:
                symbol, _, offset = part.partition(">")
                pointer = pine.read32(symbols[symbol])
                value = pine.string(pointer + int(offset, 0)) if pointer else None
                if watched.get(part) != value:
                    watched[part] = value
                    print(f"{time.time() - start:6.1f}s {part} = {value!r}", flush=True)

            if args.at_frame:
                words = [pine.read32(symbols["g_DebugStates"] + 4 * i) for i in range(64)]
                states_log = [(word >> 8, word & 0xFF) for word in words if word]

            if freeze_frame is not None and pine.read32(symbols["g_DebugFrame"]) == freeze_frame:
                # The frame's drawing finishes while the game waits
                time.sleep(1)
                take_snapshot(pine, crc(args.elf), time.time() - start, args.snapshots, f"frame_{freeze_frame}")
                for part in args.freeze_dump:
                    name, count, path = part.split(":", 2)
                    # "*SYMBOL+OFFSET": the words where the pointer there points
                    deref = name.startswith("*")
                    name, _, offset = name.lstrip("*").partition("+")
                    address = symbols[name] + (int(offset, 0) if offset else 0)
                    if deref:
                        address = pine.read32(address)
                    with open(path, "wb") as out:
                        out.write(b"".join(struct.pack("<I", pine.read32(address + 4 * i) if address else 0)
                                           for i in range(int(count, 0))))
                for part in args.freeze_range:
                    address, size, path = part.split(":", 2)
                    with open(path, "wb") as out:
                        out.write(pine.read_block(int(address, 16), int(size, 0)))
                for where, symbol, offset, count in dumps:
                    words = [pine.read32(symbols[symbol] + offset + 4 * i) for i in range(count)]
                    print(f"at frame {freeze_frame} {where}: {' '.join(f'{word:08x}' for word in words)}", flush=True)
                result = "FROZEN"
                break

            if not seen or seen[-1] != state:
                seen.append(state)
                frame_note = f" (frame {pine.read32(symbols['g_DebugFrame'])})" if args.at_frame else ""
                print(f"{time.time() - start:6.1f}s state {state}{frame_note}", flush=True)
                if state == TITLE:
                    for where, symbol, offset, count in dumps:
                        words = [pine.read32(symbols[symbol] + offset + 4 * i) for i in range(count)]
                        print(f"  {where}: {' '.join(f'{word:08x}' for word in words)}", flush=True)

            if args.pad:
                pads = pine.read32(symbols["G_GamePadController"])
                pad = pine.read32(pads + 4) if pads else 0
                value = pine.read32(pad) if pad else None
                if value != buttons:
                    buttons = value
                    print(f"{time.time() - start:6.1f}s pad 1 buttons {value if value is None else hex(value)}", flush=True)

            title_since = (title_since or time.time()) if state == TITLE else None
            if args.manual:
                if gotos and time.time() - start >= gotos[0][0]:
                    _, wanted = gotos.pop(0)
                    high = pine.read32(controller + 0xC)
                    pine.write32(controller + 0xC, high & ~(0x3F << 18) | wanted << 18)
                    print(f"{time.time() - start:6.1f}s asked for state {wanted}", flush=True)
            elif args.quick and state in NEXT:
                high = pine.read32(controller + 0xC)
                pine.write32(controller + 0xC, high & ~(0x3F << 18) | NEXT[state] << 18)
            elif state == TITLE and time.time() - title_since >= args.title_wait:
                high = pine.read32(controller + 0xC)
                pine.write32(controller + 0xC, high & ~(0x3F << 18) | NEW_GAME << 18)
                title_since = None
            elif state == PLAYING:
                if args.quick:
                    # No level title card or autosave notice
                    pine.write32(controller + 0x14, 0)

                playing_since = playing_since or time.time()
                if time.time() - playing_since > 3:
                    manager = pine.read32(symbols["G_ChunkManager"])
                    count = pine.read32(manager) & 0xFFFF
                    chunks = [pine.string(pine.read32(manager + 4 + 4 * i)) for i in range(min(count, 8))]
                    print(f"PLAYING after {time.time() - start:.0f} s, chunks loaded: {', '.join(chunks)}")
                    for where, symbol, offset, count in dumps:
                        words = [pine.read32(symbols[symbol] + offset + 4 * i) for i in range(count)]
                        print(f"{where}: {' '.join(f'{word:08x}' for word in words)}")
                    if args.check_disk:
                        check_disk_manager(pine, symbols["D_0031B4B8"])
                    if args.pad:
                        # game/pads.h: the pad's GamePadInformation
                        pads = pine.read32(symbols["G_GamePadController"])
                        info = pine.read32(pine.read32(pads + 4) + 8)
                        print(f"pad 1: setup state {pine.read32(info + 0x508)}, mode {pine.read32(info + 0x50C):#x}, "
                              f"flags {pine.read64(info + 0x568) >> 32:#x}, report mode {pine.read8(info + 0x575):#x}")
                    result = "PLAYING"
                    break

            time.sleep(0.03 if presses or (args.quick and state in (TITLE, NEW_GAME, MOVIE, START_PLAYING)) else 0.2)
        else:
            if args.manual:
                result = "MANUAL"
                # The dumps once the run's over (a manual run seldom goes through the title again)
                for where, symbol, offset, count in dumps:
                    words = [pine.read32(symbols[symbol] + offset + 4 * i) for i in range(count)]
                    print(f"at the end {where}: {' '.join(f'{word:08x}' for word in words)}", flush=True)

        if args.stay and result == "PLAYING":
            print("PCSX2 stays open, close it when done")
            return
    finally:
        if not (args.stay and result == "PLAYING"):
            for pid in (descendant(launcher.pid), launcher.pid):
                if pid:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except OSError:
                        pass

            time.sleep(1)

        placement.__exit__(None, None, None)
        if had_settings:
            shutil.move(backup, settings)
        elif os.path.exists(settings):
            os.remove(settings)

        if os.path.exists(f"{cards}/{RUN_CARD}"):
            if args.card:
                shutil.copy2(f"{cards}/{RUN_CARD}", args.card)
            os.remove(f"{cards}/{RUN_CARD}")

        for name, mtime in user_cards.items():
            if os.path.getmtime(f"{cards}/{name}") != mtime:
                print(f"WARNING: the user's memory card {name} changed during the run", flush=True)

    if args.at_frame and states_log:
        print("states by frame: " + ", ".join(f"{state}@{frame}" for frame, state in states_log))
    print(f"RESULT {result} states {seen}")
    sys.exit(0 if result in ("PLAYING", "MANUAL", "FROZEN") else 1)


main()
