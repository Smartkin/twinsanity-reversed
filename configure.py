#!/usr/bin/env python3
"""Writes build.ninja and build/compile_commands.json. The retail executable is split into asm first (tools/split.py), then
`ninja` assembles it, compiles src/ and links build/SLES_525.68.elf with PS2SDK in place of the Sony SDK functions ps2sdk.txt
lists.

    python configure.py [--matching]

--matching links the asm alone (no src/, no PS2SDK), for `ninja check`, which compares the load image with the retail one.
PLATFORM (an environment variable, ps2 by default) picks the platform layer's side in src/platform/. Where the toolchain is comes
from tools/local_config.py ($PS2DEV, local.json, else /usr/local/ps2dev or C:/ps2dev). The commands run the tools with the
Python running this script, so the same works on Linux and Windows (where ninja runs commands without a shell).
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / "tools"))
import local_config
import toolchain_dlls

WINDOWS = os.name == "nt"
PS2SDK = local_config.ps2sdk()

# The asm keeps the game's own calling convention (the EABI with 64 bit registers, float arguments from $f12 on), but it's
# assembled as n32, the ABI of the C++ and PS2SDK's libraries, since the linker takes one ABI. splat names registers the n32 way
ASFLAGS = ["-EL", "-march=r5900", "-mabi=n32", "-msingle-float", "-G0", "-no-pad-sections", "-I", "include", "-I", "."]
# C++ is PS2SDK's n32: EABI's stack is only kept 8 byte aligned by GCC, and the retail code (lq/sq) needs 16. The two agree on
# integer and pointer arguments; calls mixing ints and floats go through thunks (see docs/DEVELOPMENT.md). The retail code
# expects $f20-$f31 kept across calls, n32 only the even ones: GCC 15 accepts -fcall-saved-$f21... and still uses the odd
# ones without saving them (RigidBody::Step clobbered StepPhysicsWorld's $f21), so the C++ doesn't use them at all. C++26 for
# asm statements made of constant expressions (abi.h's thunks). The retail code's divisions don't trap on 0
CALL_SAVED_FPRS = [f"-ffixed-$f{n}" for n in range(21, 32, 2)]
CXXFLAGS = (["-march=r5900", "-mabi=n32", "-msingle-float", "-mno-abicalls", "-G0", "-O2", "-std=gnu++26", "-D_EE",
             "-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics", "-fno-asynchronous-unwind-tables", "-fno-common",
             "-fno-strict-aliasing", "-ffunction-sections", "-mno-check-zero-division", "-ffp-contract=off",
             "-fno-delete-null-pointer-checks", "-Wall", "-Wno-invalid-offsetof"] + CALL_SAVED_FPRS +
            ["-Iinclude", f"-I{PS2SDK}/ee/include", f"-I{PS2SDK}/common/include"])

PLATFORM = os.environ.get("PLATFORM", "ps2")

# PS2SDK's libraries, in place of the Sony SDK functions ps2sdk.txt lists, then the toolchain's C library (newlib's small
# libc_nano: sprintf, snprintf and string functions that do what the game's did) and GCC's runtime (__muldi3, which libmpeg
# calls). libkernel comes first: its memcpy, memset, strlen and strncpy are the game's own code, newlib's copy bytes
LIBRARIES = ["kernel", "xcdvd", "padx", "mc", "c_nano", "gcc"]


def quote(argument):
    """An argument of a ninja command: POSIX ninja runs commands through /bin/sh, Windows' straight through CreateProcess"""
    if WINDOWS:
        return f'"{argument}"' if re.search(r'[\s"]', argument) else argument

    return argument if re.fullmatch(r"[\w@%+=:,./-]+", argument) else "'" + argument.replace("'", "'\\''") + "'"


def command(arguments):
    return " ".join(quote(argument).replace("$", "$$") for argument in arguments)


def toolchain_library_dirs():
    """Where the toolchain's newlib and libgcc are: the link runs ld, which isn't told by gcc's driver"""
    dirs = []
    for name in ("libc_nano.a", "libgcc.a"):
        try:
            result = subprocess.run([local_config.ee_tool("gcc"), f"-print-file-name={name}"], capture_output=True, text=True,
                                    timeout=60)
        except (OSError, subprocess.TimeoutExpired):
            continue

        path = Path(result.stdout.strip())
        if path.is_absolute() and path.exists():
            dirs.append(Path(os.path.normpath(path.parent)).as_posix())

    return dirs


def toolchain_includes():
    """Where the toolchain's g++ looks for its own headers (the C++ library's, newlib's), for the editors' code models: clangd
    (with another target) and VS Code's C/C++ extension don't ask a cross compiler for them"""
    try:
        result = subprocess.run([local_config.ee_tool("g++"), "-E", "-v", "-x", "c++", os.devnull], capture_output=True,
                                text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return []

    lines = result.stderr.splitlines()
    if "#include <...> search starts here:" not in lines:
        return []

    start = lines.index("#include <...> search starts here:") + 1
    end = lines.index("End of search list.", start)
    return [Path(os.path.normpath(line.strip())).as_posix() for line in lines[start:end]]


def main():
    toolchain_dlls.check()
    matching = "--matching" in sys.argv[1:]
    asm = sorted((path.relative_to(HERE).as_posix() for path in (HERE / "asm").rglob("*.s")))
    # The platform layer's side for the platform built for (src/platform/<platform>/), and none of the others
    src = sorted(path.relative_to(HERE).as_posix() for path in (HERE / "src").rglob("*.cpp")
                 if path.relative_to(HERE / "src").parts[:1] != ("platform",)
                 or path.relative_to(HERE / "src").parts[1] == PLATFORM)
    if matching:
        src = []
    if not asm:
        raise SystemExit("There's no asm yet: run tools/split.py first")

    python = command([sys.executable, "-X", "utf8"])
    libs = "" if matching else " ".join("-l" + lib for lib in LIBRARIES)
    lines = [
        f"# host: {os.name}",
        "ninja_required_version = 1.10",
        f"as = {command([local_config.ee_tool('as')])}",
        f"cxx = {command([local_config.ee_tool('g++')])}",
        f"ld = {command([local_config.ee_tool('ld')])}",
        f"nm = {command([local_config.ee_tool('nm')])}",
        f"python = {python}",
        f"asflags = {command(ASFLAGS)}",
        f"libdir = {command(['-L' + directory for directory in [PS2SDK + '/ee/lib'] + toolchain_library_dirs()])}",
        f"libs = {libs}",
        f"cxxflags = {command(CXXFLAGS)}",
        "",
        "rule as",
        "  command = $as $asflags -o $out $in",
        "  description = AS $in",
        "rule cxx",
        "  command = $cxx $cxxflags -MMD -MF $out.d -c -o $out $in",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = CXX $in",
        "rule ldscript",
        f"  command = $python tools/make_ld.py $out --nm $nm {'--matching ' if matching else ''}$in",
        "  description = LDSCRIPT $out",
        "rule link",
        "  command = $ld -m elf32lr5900n32 -T build/link.ld -Map build/SLES_525.68.map -o $out $libdir --start-group $libs --end-group",
        "  description = LINK $out",
        "rule check",
        "  command = $python tools/compare.py $in SLES_525.68 build/SLES_525.68.map --touch $out",
        "  description = CHECK $in",
        "rule configure",
        f"  command = $python configure.py{' --matching' if matching else ''}",
        "  generator = 1",
        "",
    ]
    asm_objects = []
    for path in asm:
        obj = f"build/{path}.o"
        asm_objects.append(obj)
        lines.append(f"build {obj}: as {path} | include/macro.inc")

    src_objects = []
    compile_commands = []
    system_includes = [argument for path in toolchain_includes() for argument in ("-isystem", path)] if src else []
    for path in src:
        obj = f"build/{path}.o"
        src_objects.append(obj)
        lines.append(f"build {obj}: cxx {path}")
        compile_commands.append({"directory": HERE.as_posix(), "file": path, "output": obj,
                                 "arguments": [local_config.ee_tool("g++")] + CXXFLAGS + system_includes +
                                              ["-c", "-o", obj, path]})

    lines += [
        f"build build/link.ld: ldscript {' '.join(src_objects)} | tools/make_ld.py ps2sdk.txt retired.txt fragments.txt build/splat.yaml build/undefined_syms_auto.txt",
        f"build build/SLES_525.68.elf: link | build/link.ld {' '.join(asm_objects)} {' '.join(src_objects)}",
        "build build/check.ok: check build/SLES_525.68.elf",
        "build check: phony build/check.ok",
        "build build.ninja: configure | configure.py tools/local_config.py",
        "default build/SLES_525.68.elf",
        "",
    ]
    (HERE / "build.ninja").write_text("\n".join(lines), newline="\n")
    # For the editors' code models (VS Code's C/C++ extension, clangd): the C++ as it's compiled. A matching configuration keeps
    # the last one, which has every file
    if src:
        (HERE / "build").mkdir(exist_ok=True)
        (HERE / "build" / "compile_commands.json").write_text(json.dumps(compile_commands, indent=1), newline="\n")

    print(f"build.ninja: {len(asm)} asm files, {len(src)} C++ files{', matching' if matching else ''}")


main()
