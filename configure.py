#!/usr/bin/env python3
"""Writes build.ninja and build/compile_commands.json: `ninja` compiles src/ and links build/SLES_525.68.elf with PS2SDK's
libraries. With --platform desktop it writes build/desktop/build.ninja instead (`ninja -f build/desktop/build.ninja`), which
builds build/desktop/twinsanity with the host's compiler (--cxx, else $CXX, else g++) and the desktop's platform side, its window
SDL2's for 32 bit x86 (--sdl2's folder, else local_config's, else pkg-config's 32 bit sdl2; none without one).

    python configure.py [--platform ps2|desktop] [--cxx compiler] [--sdl2 folder]

Where the PS2 toolchain is comes from tools/local_config.py ($PS2DEV, local.json, else /usr/local/ps2dev or C:/ps2dev). The
commands run the tools with the Python running this script, so the same works on Linux and Windows (where ninja runs commands
without a shell).
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

# The retail executable's data, in its order (include/retaildata.h): each file's objects in the order they're written, and
# sections no longer than what's in them (the MIPS assembler pads a section to its alignment), so the next file's data starts
# where the retail data ended
RETAIL_DATA_FLAGS = ["-fno-toplevel-reorder", "-Wa,-no-pad-sections"]

# The executable keeps its relocations (--emit-relocs: sections the PS2 doesn't load), which tools/compare_builds.py reads

# The desktop: the host's compiler for 32 bit x86, where the game's structs come out as on the PS2 (4 byte pointers, 64 bit values
# aligned to 8 like the R5900's with -malign-double, DESKTOP_LAYOUT_FLAGS) and floats are SSE's single precision (x87 keeps them
# wider). The retail data stays in its order there too
DESKTOP_CXXFLAGS = ["-m32", "-fno-pie", "-msse2", "-mfpmath=sse", "-O2", "-std=gnu++26", "-fno-exceptions", "-fno-rtti",
                    "-fno-threadsafe-statics", "-fno-strict-aliasing", "-fno-delete-null-pointer-checks", "-ffp-contract=off",
                    "-Wall", "-Wno-invalid-offsetof", "-Iinclude"]
# What lays the game's structs out and calls its functions as the PS2 build does, which the SDL2 files (SDL2_SOURCES) leave out:
# theirs are laid out and called as the SDL library has them. 32 bit Windows lays bitfields out its own way and passes a member
# function's this in ECX (thiscall), so the game takes System V's i386 conventions there as everywhere else (the retail vtables
# are called as plain functions with this first); the desktop side only calls C there (the C library's, the system's), not
# libstdc++'s compiled code, which has Windows' conventions
DESKTOP_LAYOUT_FLAGS = ["-malign-double"]
DESKTOP_WINDOWS_LAYOUT_FLAGS = ["-mabi=sysv", "-mno-ms-bitfields"]
# GCC needs telling to keep a file's objects in their order, Clang keeps them so and doesn't take the option
DESKTOP_RETAIL_DATA_FLAGS = ["-fno-toplevel-reorder"]
# The retail data's files, linked first in the order the PS2's linker script puts their sections in (include/retaildata.h)
DESKTOP_RETAIL_DATA_ORDER = ["src/data/data.cpp", "src/data/rodata.cpp", "src/data/sdata.cpp", "src/data/sbss.cpp",
                             "src/data/bss.cpp"]
# Debug information: GCC's whole, Clang's line tables (its whole debug information gives the explicit specialisations of a class
# template's members after the first one defined their C++ names instead of their retail ones). Clang warns about the GCC 2.9x
# destructors' checks of this for null, which -fno-delete-null-pointer-checks keeps
DESKTOP_GCC_FLAGS = ["-g"]
DESKTOP_CLANG_FLAGS = ["-gline-tables-only", "-Wno-tautological-undefined-compare"]
# libgcc linked in: nothing throws, and Clang doesn't find the 32 bit shared one where GCC's 64 bit side keeps its link
DESKTOP_LDFLAGS = ["-m32", "-no-pie", "-static-libgcc"]
# Windows: MinGW's runtime linked in, so only SDL2.dll goes next to the executable, and the system timer's steps (winmm)
DESKTOP_WINDOWS_LDFLAGS = ["-m32", "-static"]
DESKTOP_WINDOWS_LIBS = ["-lwinmm"]

PLATFORM = os.environ.get("PLATFORM", "ps2")
if "--platform" in sys.argv[1:]:
    PLATFORM = sys.argv[sys.argv.index("--platform") + 1]


def is_retail_data(path):
    """src/data/ and the platform side's retail data (the PS2's VU microcode and its programs' sizes)"""
    return path.startswith("src/data/") or path in ("src/platform/ps2/renderer/vumicrocode.cpp",
                                                    "src/platform/ps2/renderer/vuprogramsizes.cpp")

# PS2SDK's libraries, in place of Sony's SDK, then the toolchain's C library (newlib's small libc_nano: sprintf, snprintf and
# string functions that do what the game's did) and GCC's runtime (__muldi3, which libmpeg calls). libkernel comes first: its
# memcpy, memset, strlen and strncpy are the game's own code, newlib's copy bytes
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


def sources():
    """src/'s C++ for the platform built for: its side of the platform layer (src/platform/<platform>/), none of the others"""
    return sorted(path.relative_to(HERE).as_posix() for path in (HERE / "src").rglob("*.cpp")
                  if path.relative_to(HERE / "src").parts[:1] != ("platform",)
                  or path.relative_to(HERE / "src").parts[1] == PLATFORM)


def is_clang(cxx):
    try:
        result = subprocess.run([cxx, "--version"], capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return False

    return "clang" in result.stdout


def is_windows_target(cxx):
    """Whether the compiler makes Windows programs (MinGW's), by its target: the host's system when it doesn't say"""
    try:
        machine = subprocess.run([cxx, "-dumpmachine"], capture_output=True, text=True, timeout=60).stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        machine = ""

    if not machine:
        return WINDOWS

    return any(name in machine for name in ("mingw", "windows", "cygwin"))


def ninja_path(path):
    """A path in a build statement, with ninja's escapes for its spaces, colons and dollars"""
    return path.replace("$", "$$").replace(" ", "$ ").replace(":", "$:")


# pkg-config's folders of 32 bit x86 packages (Arch's lib32, Debian's multiarch), unless PKG_CONFIG_LIBDIR says otherwise
SDL2_PKG_CONFIG_LIBDIR = "/usr/lib32/pkgconfig:/usr/lib/i386-linux-gnu/pkgconfig"
# SDL2's MinGW development release has a folder for each target, this is the 32 bit one; tools/fetch_sdl2.py puts it in build/sdl2
SDL2_MINGW_FOLDER = "i686-w64-mingw32"
SDL2_FETCHED = HERE / "build" / "sdl2" / SDL2_MINGW_FOLDER
# What SDL2's pkg-config file adds for programs whose main SDL2main wraps in Windows' GUI programs' WinMain: the desktop has its
# own main (SDL_MAIN_HANDLED) and stays a console program
SDL2_MAIN_FLAGS = ["-Dmain=SDL_main", "-lSDL2main", "-mwindows"]
# The desktop's file using SDL2 (its window), built for the host's own layouts (DESKTOP_LAYOUT_FLAGS)
SDL2_SOURCES = ["src/platform/desktop/window.cpp"]


def sdl2_folder(windows):
    """A folder of the desktop's SDL2 (include/SDL2, lib, and on Windows bin): --sdl2's, else local_config's, else for Windows
    the one tools/fetch_sdl2.py fetched. SDL2's MinGW development release can be given whole"""
    if "--sdl2" in sys.argv[1:]:
        folder = Path(sys.argv[sys.argv.index("--sdl2") + 1])
    elif local_config.sdl2():
        folder = Path(local_config.sdl2())
    elif windows and SDL2_FETCHED.exists():
        folder = SDL2_FETCHED
    else:
        return None

    if (folder / SDL2_MINGW_FOLDER / "include" / "SDL2").exists():
        folder = folder / SDL2_MINGW_FOLDER

    return folder.as_posix()


def pkg_config(argument, windows):
    """What pkg-config gives of SDL2 (the 32 bit x86 package elsewhere than Windows), split"""
    environment = dict(os.environ)
    if not windows and not WINDOWS:
        environment.setdefault("PKG_CONFIG_LIBDIR", SDL2_PKG_CONFIG_LIBDIR)

    return subprocess.run(["pkg-config", argument, "sdl2"], env=environment, capture_output=True, text=True, check=True,
                          timeout=60).stdout.split()


def find_sdl2(folder, windows):
    """The desktop's SDL2 for 32 bit x86, the folder's, else pkg-config's: its compile flags, its libraries and on Windows the
    DLL that goes next to the executable (linked through its import library by name: -static would take libSDL2.a, which needs
    Windows' libraries of its own). None without one"""
    library = "-l:libSDL2.dll.a" if windows else "-lSDL2"
    if folder:
        libs = [f"-L{folder}/lib", library] + ([] if windows else [f"-Wl,-rpath,{folder}/lib"])
        return [f"-I{folder}/include/SDL2"], libs, f"{folder}/bin/SDL2.dll" if windows else None

    try:
        flags = pkg_config("--cflags", windows)
        libs = pkg_config("--libs", windows)
        prefix = pkg_config("--variable=prefix", windows)
    except (OSError, subprocess.SubprocessError):
        return None

    flags = [flag for flag in flags if flag not in SDL2_MAIN_FLAGS]
    libs = [library if lib == "-lSDL2" else lib for lib in libs if lib not in SDL2_MAIN_FLAGS]
    return flags, libs, f"{prefix[0]}/bin/SDL2.dll" if windows and prefix else None


class DesktopSetup:
    """The desktop build's compiler (--cxx, else $CXX, else g++), whether it makes Windows programs, and flags: every file's, the
    retail data's added ones, the SDL2 files' own (SDL2's when there's one), the link's and the libraries"""

    def __init__(self):
        self.cxx = sys.argv[sys.argv.index("--cxx") + 1] if "--cxx" in sys.argv[1:] else os.environ.get("CXX", "g++")
        clang = is_clang(self.cxx)
        self.windows = is_windows_target(self.cxx)
        compiler = DESKTOP_CLANG_FLAGS if clang else DESKTOP_GCC_FLAGS
        layout = DESKTOP_LAYOUT_FLAGS + (DESKTOP_WINDOWS_LAYOUT_FLAGS if self.windows else [])
        self.cxxflags = DESKTOP_CXXFLAGS + layout + compiler
        self.retail_data_flags = [] if clang else DESKTOP_RETAIL_DATA_FLAGS
        self.sdl2_folder = sdl2_folder(self.windows)
        sdl2 = find_sdl2(self.sdl2_folder, self.windows)
        self.has_sdl2 = sdl2 is not None
        self.sdl2_cxxflags = DESKTOP_CXXFLAGS + compiler + (["-DDESKTOP_SDL2"] + sdl2[0] if sdl2 else [])
        self.sdl2_dll = sdl2[2] if sdl2 else None
        self.ldflags = DESKTOP_WINDOWS_LDFLAGS if self.windows else DESKTOP_LDFLAGS
        self.libs = (sdl2[1] if sdl2 else []) + (DESKTOP_WINDOWS_LIBS if self.windows else [])
        self.executable = "twinsanity" + (".exe" if self.windows else "")

    def flags(self, path):
        if is_retail_data(path):
            return self.cxxflags + self.retail_data_flags

        return self.sdl2_cxxflags if path in SDL2_SOURCES else self.cxxflags


def desktop_platform_sources():
    """The desktop's side of the platform layer, which the PS2's compile commands have too (for the editors, .clangd)"""
    return sorted(path.relative_to(HERE).as_posix() for path in (HERE / "src" / "platform" / "desktop").rglob("*.cpp"))


def configure_desktop():
    desktop = DesktopSetup()
    src = sources()
    retail = [path for path in DESKTOP_RETAIL_DATA_ORDER if path in src]
    src = retail + [path for path in src if path not in retail]
    out_dir = "build/desktop"
    executable = f"{out_dir}/{desktop.executable}"
    configure = ["--platform", "desktop", "--cxx", desktop.cxx]
    configure += ["--sdl2", desktop.sdl2_folder] if desktop.sdl2_folder else []
    lines = [
        f"# host: {os.name}",
        "ninja_required_version = 1.10",
        f"builddir = {out_dir}",
        f"cxx = {command([desktop.cxx])}",
        f"cxxflags = {command(desktop.cxxflags)}",
        f"retaildataflags = {command(desktop.retail_data_flags)}",
        f"sdl2flags = {command(desktop.sdl2_cxxflags)}",
        f"ldflags = {command(desktop.ldflags)}",
        f"libs = {command(desktop.libs)}",
        "",
        "rule cxx",
        "  command = $cxx $cxxflags -MMD -MF $out.d -c -o $out $in",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = CXX $in",
        "rule link",
        "  command = $cxx $ldflags -o $out $in $libs",
        "  description = LINK $out",
        "rule copy",
        f"  command = $python -c {quote('import shutil, sys; shutil.copyfile(sys.argv[1], sys.argv[2])')} $in $out",
        "  description = COPY $out",
        "rule configure",
        f"  command = $python configure.py {command(configure)}",
        "  generator = 1",
        f"python = {command([sys.executable, '-X', 'utf8'])}",
        "",
    ]
    objects = []
    for path in src:
        obj = f"{out_dir}/{path}.o"
        objects.append(obj)
        lines.append(f"build {obj}: cxx {path}")
        if is_retail_data(path):
            lines.append("  cxxflags = $cxxflags $retaildataflags")
        elif path in SDL2_SOURCES:
            lines.append("  cxxflags = $sdl2flags")

    lines.append(f"build {executable}: link {' '.join(objects)}")
    defaults = [executable]
    if desktop.sdl2_dll:
        lines.append(f"build {out_dir}/SDL2.dll: copy {ninja_path(desktop.sdl2_dll)}")
        defaults.append(f"{out_dir}/SDL2.dll")

    lines += [
        f"build {out_dir}/build.ninja: configure | configure.py tools/local_config.py",
        f"default {' '.join(defaults)}",
        "",
    ]
    (HERE / out_dir).mkdir(parents=True, exist_ok=True)
    (HERE / out_dir / "build.ninja").write_text("\n".join(lines), newline="\n")
    print(f"{out_dir}/build.ninja: {len(src)} C++ files")
    if not desktop.has_sdl2:
        where = ("tools/fetch_sdl2.py fetches SDL2's MinGW development release" if desktop.windows
                 else f"else pkg-config's sdl2 in {SDL2_PKG_CONFIG_LIBDIR}")
        print(f"No 32 bit x86 SDL2 (--sdl2, local.json's \"sdl2\" or $SDL2; {where}): the desktop build opens no window")


def main():
    if PLATFORM == "desktop":
        configure_desktop()
        return

    toolchain_dlls.check()
    src = sources()

    python = command([sys.executable, "-X", "utf8"])
    libs = " ".join("-l" + lib for lib in LIBRARIES)
    lines = [
        f"# host: {os.name}",
        "ninja_required_version = 1.10",
        f"cxx = {command([local_config.ee_tool('g++')])}",
        f"ld = {command([local_config.ee_tool('ld')])}",
        f"nm = {command([local_config.ee_tool('nm')])}",
        f"python = {python}",
        f"libdir = {command(['-L' + directory for directory in [PS2SDK + '/ee/lib'] + toolchain_library_dirs()])}",
        f"libs = {libs}",
        f"cxxflags = {command(CXXFLAGS)}",
        f"retaildataflags = {command(RETAIL_DATA_FLAGS)}",
        "",
        "rule cxx",
        "  command = $cxx $cxxflags -MMD -MF $out.d -c -o $out $in",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = CXX $in",
        "rule ldscript",
        "  command = $python tools/make_ld.py $out $in",
        "  description = LDSCRIPT $out",
        "rule link",
        "  command = $ld -m elf32lr5900n32 -T build/link.ld -Map build/SLES_525.68.map --emit-relocs -o $out $libdir --start-group $libs --end-group",
        "  description = LINK $out",
        "rule configure",
        "  command = $python configure.py",
        "  generator = 1",
        "",
    ]
    src_objects = []
    compile_commands = []
    system_includes = [argument for path in toolchain_includes() for argument in ("-isystem", path)]
    for path in src:
        obj = f"build/{path}.o"
        src_objects.append(obj)
        lines.append(f"build {obj}: cxx {path}")
        flags = CXXFLAGS
        if is_retail_data(path):
            lines.append("  cxxflags = $cxxflags $retaildataflags")
            flags = CXXFLAGS + RETAIL_DATA_FLAGS
        compile_commands.append({"directory": HERE.as_posix(), "file": path, "output": obj,
                                 "arguments": [local_config.ee_tool("g++")] + flags + system_includes +
                                              ["-c", "-o", obj, path]})

    desktop = DesktopSetup()
    for path in desktop_platform_sources():
        obj = f"build/desktop/{path}.o"
        compile_commands.append({"directory": HERE.as_posix(), "file": path, "output": obj,
                                 "arguments": [desktop.cxx] + desktop.flags(path) + ["-c", "-o", obj, path]})

    lines += [
        f"build build/link.ld: ldscript {' '.join(src_objects)} | tools/make_ld.py",
        f"build build/SLES_525.68.elf: link | build/link.ld {' '.join(src_objects)}",
        "build build.ninja: configure | configure.py tools/local_config.py",
        "default build/SLES_525.68.elf",
        "",
    ]
    (HERE / "build.ninja").write_text("\n".join(lines), newline="\n")
    # For the editors' code models (VS Code's C/C++ extension, clangd): the C++ as it's compiled
    (HERE / "build").mkdir(exist_ok=True)
    (HERE / "build" / "compile_commands.json").write_text(json.dumps(compile_commands, indent=1), newline="\n")

    print(f"build.ninja: {len(src)} C++ files")


main()
