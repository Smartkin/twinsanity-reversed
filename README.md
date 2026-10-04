# Crash Twinsanity decompilation (PAL, SLES-525.68)

Crash Twinsanity's PAL release for the PlayStation 2, rewritten in C++. Every function of the retail executable is C++ now, or
PS2SDK's and the toolchain's where they do what Sony's SDK did, and the build boots and plays in PCSX2 the way the retail game does:
the same heap and disk state at boot, the same pictures frame for frame, the same saves. The C++ is built with the open source
PS2SDK toolchain, and the game's C++ reaches the hardware through a platform layer so it can be ported.

The retail executable is still split, into one asm file per function and its data: the build links the data (`.data`, `.rodata`,
`.bss`, the VU microcode) from the split, and the functions' asm builds the matching executable (`tools/build.py --matching`) that
the C++ was checked against. The repository has none of the game's code or data. You need your own copy of the PAL game: building
splits its executable on your machine.

[docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) explains how the decomp is put together, the tools that test builds and what replacing
the game's code had to look out for. [docs/RETAIL_BUGS.md](docs/RETAIL_BUGS.md) lists the bugs of the retail code the C++ keeps.

## What you need

- **The game**: the PAL disc (SLES-52568). Building needs its executable, `SLES_525.68` from the disc's root folder, and playing
  the build needs an image of the disc.
- **The PS2SDK toolchain** from [ps2dev](https://github.com/ps2dev/ps2dev) (GCC 15.2). Its prebuilt releases have one for Linux
  and one for Windows.
- **Python 3.10 or later**, for splat (which splits the executable), ninja and the build's scripts.
- **Git**, to clone this repository: `git clone https://github.com/Smartkin/twinsanity-reversed.git`
- **[PCSX2](https://pcsx2.net/)**, to play the build.

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

3. Take `SLES_525.68` out of your disc image into the repository's folder and check that it's the right one. `bsdtar` is
   libarchive's tar, which reads disc images (GNU tar doesn't); `7z e` works too, and so does a symlink to a copy you have.

   ```sh
   bsdtar -xf "/path/to/Crash Twinsanity (Europe).iso" SLES_525.68
   sha1sum SLES_525.68    # d41b8d53f733f930cf03d8ea944bec24429fe57c
   ```

4. Split it into `asm/` (once) and build:

   ```sh
   .venv/bin/python tools/split.py
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

4. Take `SLES_525.68` out of your disc image into the repository's folder and check that it's the right one. Windows' `tar` reads
   disc images, or open the image in Explorer, which mounts it as a drive, and copy the file.

   ```powershell
   tar -xf "C:\path\to\Crash Twinsanity (Europe).iso" SLES_525.68
   Get-FileHash -Algorithm SHA1 SLES_525.68    # D41B8D53F733F930CF03D8EA944BEC24429FE57C
   ```

5. Split it into `asm\` (once) and build:

   ```powershell
   .venv\Scripts\python.exe tools\split.py
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

Write Windows paths in it with forward slashes (`C:/Games/Twinsanity.iso`) or with doubled backslashes.

## Building and playing

The scripts run with the environment's Python (`.venv/bin/python`, `.venv\Scripts\python.exe` on Windows), from the
repository's folder:

| Command | What it does |
|---|---|
| `tools/build.py` | Builds `build/SLES_525.68.elf`, writing `build.ninja` first when there's none for this system. Other arguments go to ninja (`-j8`, `-v`) |
| `tools/build.py --clean` | Deletes what the build made |
| `tools/build.py --matching` | Builds the asm alone and checks that it gives the retail executable's load image back, byte for byte |
| `tools/play.py` | Boots the build in PCSX2 with your disc image; stopping it (Ctrl+C) closes PCSX2. `--elf`, `--iso` and `--pcsx2` pick others |
| `tools/split.py` | Splits `SLES_525.68` into `asm/` and `assets/` again |
| `configure.py` | Writes `build.ninja` and `build/compile_commands.json` again |

Run `configure.py` again when C++ files were added or removed (a pull can bring new ones), and `tools/split.py` when
`symbol_addrs.txt` changed, since the asm takes its names from it.

On Linux, `tools/run_pcsx2.py` and `tools/render_check.py` test builds in PCSX2's Flatpak: see
[Testing in PCSX2](docs/DEVELOPMENT.md#testing-in-pcsx2).

## Editor setup

[clangd](https://clangd.llvm.org/) gives the C++ its completion and errors: `.clangd` points it at
`build/compile_commands.json`, which `configure.py` writes, and gives clang a MIPS target in place of the R5900, which it doesn't
know. The structs come out the same, so the size checks hold in the editor too. In VS Code that's the clangd extension
(`llvm-vs-code-extensions.vscode-clangd`, also on Open VSX).

Microsoft's C/C++ extension works with the same file (`"compileCommands"` in `c_cpp_properties.json`) but lays structs out for
x86, so `include/common.h` and `include/abi.h` leave the size checks and the EABI thunks out for it (`__INTELLISENSE__`). Use one
of the two extensions, not both.

A VS Code build task only needs to run `tools/build.py` with the `$gcc` problem matcher and file locations relative to the
folder.

## Layout

| Path | What |
|---|---|
| `src/game/` | The game's C++ |
| `src/platform/` | The platform layer's side for each platform (`ps2/` on PS2SDK) |
| `src/abi.cpp` | The calls between the retail code's convention and the C++'s |
| `include/` | The game's types, the platform layer's interfaces and the asm's macros |
| `tools/` | The split, the build, the checks and the PCSX2 runners |
| `symbol_addrs.txt` | The executable's symbols, which name the asm, made from the Ghidra project |
| `splat.yaml` | The split's settings |
| `ps2sdk.txt`, `retired.txt`, `fragments.txt` | Sony's and the game's functions left out of the link, and the bytes between functions nothing reaches |
| `docs/DEVELOPMENT.md` | How it all works |
| `docs/RETAIL_BUGS.md` | The retail code's bugs, verified in its asm and kept by the C++ |

`asm/`, `assets/` and `build/` are made by the split and the build, and aren't committed.
