# Crash Twinsanity decompilation (PAL, SLES-525.68)

Crash Twinsanity's PAL release for the PlayStation 2, rewritten in C++. Every function of the retail executable is C++ now, or
PS2SDK's and the toolchain's where they do what Sony's SDK did, and the build boots and plays in PCSX2 the way the retail game does:
the same heap and disk state at boot, the same pictures frame for frame, the same saves. The C++ is built with the open source
PS2SDK toolchain, and the game's C++ reaches the hardware through a platform layer so it can be ported.

This branch (`elfree`) builds from the repository alone: the retail executable's data (its tables, texts, vtables and the VU
microcode) is C++ in `src/data/` and the PS2 side's renderer, in the retail order, so nothing is split from the game's executable
and the build is the same program as one made from the split (`tools/compare_builds.py` checks it). Playing the build still needs
your own copy of the PAL game, an image of its disc. The branch also has the start of a desktop port: a 32 bit x86 build of the
same C++ with a desktop side of the platform layer (see [The desktop](#the-desktop)).

[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) explains how the decomp is put together, the tools that test builds and what replacing
the game's code had to look out for. [docs/RETAIL_BUGS.md](docs/RETAIL_BUGS.md) lists the bugs of the retail code the C++ keeps.

## What you need

- **The PS2SDK toolchain** from [ps2dev](https://github.com/ps2dev/ps2dev) (GCC 15.2). Its prebuilt releases have one for Linux
  and one for Windows.
- **Python 3.10 or later**, for ninja and the build's scripts.
- **Git**, to clone this repository: `git clone https://github.com/Smartkin/twinsanity-reversed.git`
- **[PCSX2](https://pcsx2.net/)** and an image of the PAL disc (SLES-52568), to play the build.

## Building on Linux

1. Download `ps2dev-ubuntu-latest.tar.gz` from [ps2dev's latest release](https://github.com/ps2dev/ps2dev/releases/tag/latest)
   and extract it into `/usr/local`, which makes `/usr/local/ps2dev`:

   ```sh
   sudo tar -xzf ps2dev-ubuntu-latest.tar.gz -C /usr/local
   ```

   A toolchain somewhere else (or one you built) works too: put its folder in `PS2DEV` or in `local.json` (see
   [Settings](#settings)).

2. In the repository's folder, make the Python environment:

   ```sh
   python3 -m venv .venv
   .venv/bin/python -m pip install -r requirements.txt
   ```

3. Build:

   ```sh
   .venv/bin/python tools/build.py
   ```

   The build is `build/SLES_525.68.elf`.

## Building on Windows

Run these in PowerShell, in the repository's folder.

1. Download `ps2dev-windows-latest.tar.gz` from [ps2dev's latest release](https://github.com/ps2dev/ps2dev/releases/tag/latest)
   and extract it into `C:\` with the `tar` that comes with Windows, which makes `C:\ps2dev`:

   ```powershell
   tar -xzf "$HOME\Downloads\ps2dev-windows-latest.tar.gz" -C C:\
   ```

   Nothing has to go on the `PATH`. A toolchain somewhere else goes in `PS2DEV` or in `local.json` (see [Settings](#settings)).

2. Install Python from [python.org](https://www.python.org/downloads/windows/), which comes with the `py` launcher, and make the
   Python environment:

   ```powershell
   py -3 -m venv .venv
   .venv\Scripts\python.exe -m pip install -r requirements.txt
   ```

3. Give the toolchain the DLLs it needs. ps2dev builds it in MSYS2 and leaves out MSYS2's DLLs its programs use
   (`libwinpthread-1.dll`, `libiconv-2.dll`, GCC's and binutils' libraries), so without them Windows says they weren't found.
   This downloads them from MSYS2's package repository and puts them next to the programs (again whenever you replace the
   toolchain):

   ```powershell
   .venv\Scripts\python.exe tools\toolchain_dlls.py
   ```

4. Build:

   ```powershell
   .venv\Scripts\python.exe tools\build.py
   ```

   The build is `build\SLES_525.68.elf`.

## Settings

`local.json` in the repository's folder keeps your machine's paths. It isn't committed; copy `local.example.json` to start one.
An environment variable wins over it, and without either the default is used:

| Key | Variable | What | Default |
|---|---|---|---|
| `ps2dev` | `PS2DEV` | The toolchain's folder | `/usr/local/ps2dev`, `C:/ps2dev` on Windows |
| `ps2sdk` | `PS2SDK` | PS2SDK's folder | `ps2sdk` in the toolchain's folder |
| `disc_image` | `TWINSANITY_ISO` | The PAL disc image to play the build with | none |
| `pcsx2` | `PCSX2` | PCSX2's executable, or `flatpak` for PCSX2's Flatpak | the Flatpak, `pcsx2-qt` on the `PATH`, PCSX2's install folder on Windows |
| `sdl2` | `SDL2` | A 32 bit x86 SDL2 for the desktop build's window (its `include/SDL2` and `lib`) | pkg-config's 32 bit `sdl2` |

Write Windows paths in it with forward slashes (`C:/Games/Twinsanity.iso`) or with doubled backslashes.

## Building and playing

The scripts run with the environment's Python (`.venv/bin/python`, `.venv\Scripts\python.exe` on Windows), from the
repository's folder:

| Command | What it does |
|---|---|
| `tools/build.py` | Builds `build/SLES_525.68.elf`, writing `build.ninja` first when there's none for this system. Other arguments go to ninja (`-j8`, `-v`) |
| `tools/build.py --clean` | Deletes what the build made |
| `tools/build.py --platform desktop` | Builds the desktop's `build/desktop/twinsanity` (see [The desktop](#the-desktop)) |
| `tools/play.py` | Boots the build in PCSX2 with your disc image; stopping it (Ctrl+C) closes PCSX2. `--elf`, `--iso` and `--pcsx2` pick others |
| `configure.py` | Writes `build.ninja` and `build/compile_commands.json` again (`--platform desktop`: `build/desktop/build.ninja`, `--cxx` and `--sdl2` pick the compiler and SDL2) |

Run `configure.py` again when C++ files were added or removed (a pull can bring new ones).

On Linux, `tools/run_pcsx2.py` and `tools/render_check.py` test builds in PCSX2's Flatpak: see
[Testing in PCSX2](docs/DEVELOPMENT.md#testing-in-pcsx2).

## The desktop

`tools/build.py --platform desktop` builds the same C++ with the host's compiler (`g++` by default; `clang++` works too) for 32
bit x86, with the desktop's side of the platform layer (`src/platform/desktop/`) in place of the PS2's. On 32 bit x86 with
`-malign-double` every struct is laid out as on the PS2 (4 byte pointers, 64 bit values aligned to 8), so the size checks hold and
the retail data is the same. It needs the host compiler's 32 bit support (GCC's or Clang's multilib) and, for its window, SDL2 for
32 bit x86 (Arch: `lib32-sdl2-compat`; Debian and Ubuntu: `libsdl2-dev:i386`; or a folder of one, the `sdl2` setting). Another
compiler or SDL2 is picked once, before the first build:

    python configure.py --platform desktop --cxx clang++ --sdl2 /path/to/SDL2

It runs with the PAL disc's files in a folder (`TWINSANITY_DISC=/path/to/disc build/desktop/twinsanity`, the folder with
`Crash6`). The desktop side is stubs for now, the parts the game's results depend on aside: the maths the PS2 does on its vector
unit 0 (its microprograms and macro mode: the sines and cosines, the joints' animations, the matrix and collision helpers, the ray
tests, the culling, the decals) is C++. It opens its window and runs the game's loop at 60 frames a second, but nothing is drawn,
played or streamed yet: the game waits on its legal screen. See [the porting notes](docs/DEVELOPMENT.md#the-desktop-port).

## Editor setup

[clangd](https://clangd.llvm.org/) gives the C++ its completion and errors: `.clangd` points it at
`build/compile_commands.json`, which `configure.py` writes, gives clang a MIPS target in place of the R5900, which it doesn't
know, and leaves out the GCC options clang doesn't take (the retail data's `-fno-toplevel-reorder` among them). The structs come out the same, so the size checks hold in the editor too. In VS Code that's the clangd extension
(`llvm-vs-code-extensions.vscode-clangd`, also on Open VSX).

Microsoft's C/C++ extension works with the same file (`"compileCommands"` in `c_cpp_properties.json`) but lays structs out for
x86, so `include/common.h` and `include/abi.h` leave the size checks and the EABI thunks out for it (`__INTELLISENSE__`). Use one
of the two extensions, not both.

A VS Code build task only needs to run `tools/build.py` with the `$gcc` problem matcher and file locations relative to the
folder.

The desktop build runs under gdb from a `cppdbg` launch configuration (Microsoft's C/C++ extension in VS Code; in Code - OSS and
VSCodium the C/C++ Debug extension, `kylinideteam.cppdebug` on Open VSX): `"program"` is `build/desktop/twinsanity`, its
`"environment"` has `TWINSANITY_DISC` (the extracted PAL disc's folder) and its `"preLaunchTask"` runs `configure.py --platform
desktop`, then `tools/build.py --platform desktop` (configuring each time picks up an SDL2 installed since).

## Layout

| Path | What |
|---|---|
| `src/game/` | The game's C++ |
| `src/data/` | The retail executable's data, in its order and under its names |
| `src/retail/` | The game's C library as Sony's newlib had it (its heap, qsort, rand, expf) |
| `src/platform/` | The platform layer's side for each platform (`ps2/` on PS2SDK, `desktop/` for 32 bit x86) |
| `src/abi.cpp` | The calls between the retail code's convention and the C++'s (the PS2's) |
| `include/` | The game's types and the platform layer's interfaces |
| `tools/` | The build, the checks and the PCSX2 runners |
| `symbol_addrs.txt` | The retail executable's names and their addresses in it (the C++ links by them; the build doesn't read it) |
| `docs/DEVELOPMENT.md` | How it all works |
| `docs/RETAIL_BUGS.md` | The retail code's bugs, verified in its asm and kept by the C++ |

`build/` is made by the build, and isn't committed.
