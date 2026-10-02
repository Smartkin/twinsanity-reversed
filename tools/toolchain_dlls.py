#!/usr/bin/env python3
"""Puts the DLLs the Windows toolchain's programs need next to them.

ps2dev's Windows release (ps2dev-windows-latest.tar.gz) is built in MSYS2's MINGW32 environment and leaves MSYS2's DLLs out:
libwinpthread-1.dll and libiconv-2.dll for g++, gmp, mpfr, mpc and isl for cc1plus, libzstd.dll for binutils. Its programs
only start where MSYS2's mingw32/bin is on the PATH. This reads which DLLs the programs of the toolchain's EE folders import,
downloads the MSYS2 packages that have them from the repository the programs were built in (checked against the SHA-256 its
database gives), and copies the DLLs next to the programs, the first place Windows looks for them. DLLs already there are
kept, unless they're built for another machine (a 64 bit one next to the 32 bit programs). Run it again after replacing the
toolchain.

    python tools/toolchain_dlls.py

The toolchain's folder comes from tools/local_config.py ($PS2DEV, local.json, C:/ps2dev). The packages are kept in
build/msys2/. configure.py and build.py stop with what to do when the build's programs couldn't start (check()).
"""
import hashlib
import io
import os
import struct
import sys
import tarfile
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import local_config

HERE = Path(__file__).resolve().parent.parent
TARGET = "mips64r5900el-ps2-elf"
REPOSITORIES = "https://repo.msys2.org/mingw"
I386 = 0x14C


def pe_imports(path):
    """A Windows program's or DLL's machine and the DLLs it imports (delay loaded ones too), in lower case"""
    data = Path(path).read_bytes()
    header = struct.unpack_from("<I", data, 0x3C)[0] if data[:2] == b"MZ" else 0
    if data[header:header + 4] != b"PE\0\0":
        raise ValueError(f"{path} isn't a Windows program")

    machine, section_count = struct.unpack_from("<HH", data, header + 4)
    optional_size = struct.unpack_from("<H", data, header + 20)[0]
    optional = header + 24
    pe32 = struct.unpack_from("<H", data, optional)[0] == 0x10B
    image_base = struct.unpack_from("<I" if pe32 else "<Q", data, optional + (28 if pe32 else 24))[0]
    directory_count = struct.unpack_from("<I", data, optional + (92 if pe32 else 108))[0]
    directories = optional + (96 if pe32 else 112)
    sections = [struct.unpack_from("<IIII", data, optional + optional_size + index * 40 + 8)
                for index in range(section_count)]

    def offset(rva):
        for size, address, raw_size, raw_offset in sections:
            if address <= rva < address + max(size, raw_size):
                return rva - address + raw_offset
        raise ValueError(f"{path}: address {rva:#x} is in no section")

    imports = []
    # Directory 1 is the import table (20 byte entries, the DLL's name at 12), 13 the delay import table (32 byte entries,
    # attributes first, the name at 4; without attribute bit 0 its addresses aren't relative to the image)
    for directory, entry_size, name_at in ((1, 20, 12), (13, 32, 4)):
        rva = struct.unpack_from("<I", data, directories + directory * 8)[0] if directory < directory_count else 0
        if not rva:
            continue

        position = offset(rva)
        while name := struct.unpack_from("<I", data, position + name_at)[0]:
            if directory == 13 and not struct.unpack_from("<I", data, position)[0] & 1:
                name -= image_base
            start = offset(name)
            imports.append(data[start:data.index(b"\0", start)].decode("ascii").lower())
            position += entry_size

    return machine, imports


def folders(root):
    """The toolchain's folders of EE programs: its tools, the copies g++ runs and GCC's own programs"""
    ee = Path(root) / "ee"
    candidates = [ee / "bin", ee / TARGET / "bin"] + sorted((ee / "libexec" / "gcc" / TARGET).glob("*"))
    return [folder for folder in candidates if folder.is_dir() and any(folder.glob("*.exe"))]


def repository(machine, imports):
    """The MSYS2 repository a program was built in: MINGW32 for 32 bits, UCRT64 or MINGW64 for 64 by their C library"""
    if machine == I386:
        return "mingw32"

    if any(name == "ucrtbase.dll" or name.startswith("api-ms-win-crt-") for name in imports):
        return "ucrt64"

    return "mingw64"


def download(url):
    with urllib.request.urlopen(url, timeout=120) as response:
        return response.read()


def decompress(data):
    """What a zstd file holds: Python 3.14's compression.zstd, else the zstandard package (requirements.txt)"""
    try:
        from compression import zstd
        return zstd.decompress(data)
    except ImportError:
        pass

    try:
        import zstandard
    except ImportError:
        raise SystemExit("Reading MSYS2's packages takes Python 3.14 or the zstandard package: "
                         "python -m pip install -r requirements.txt")

    with zstandard.ZstdDecompressor().stream_reader(io.BytesIO(data), read_across_frames=True) as reader:
        return reader.read()


class Repository:
    """An MSYS2 repository's DLLs by their name, from its files database (each package's files, file name and SHA-256)"""

    def __init__(self, name, cache):
        self.name = name
        self.cache = cache
        self.dlls = {}
        self.packages = {}
        self.opened = {}
        print(f"Reading MSYS2's {name} repository", flush=True)
        database = tarfile.open(fileobj=io.BytesIO(decompress(download(f"{REPOSITORIES}/{name}/{name}.files"))))
        for member in database.getmembers():
            package, _, kind = member.name.partition("/")
            if kind == "desc":
                self.packages[package] = fields(database.extractfile(member).read().decode())
            elif kind == "files":
                for line in database.extractfile(member).read().decode().splitlines():
                    if line.lower().startswith(f"{name}/bin/") and line.lower().endswith(".dll"):
                        self.dlls.setdefault(line.rsplit("/", 1)[1].lower(), (package, line))

    def dll(self, name):
        """The DLL's bytes, from its package (downloaded once, kept in the cache)"""
        package, member = self.dlls[name]
        if package not in self.opened:
            description = self.packages[package]
            filename, digest = description["FILENAME"][0], description["SHA256SUM"][0]
            path = self.cache / filename
            data = path.read_bytes() if path.exists() else b""
            if hashlib.sha256(data).hexdigest() != digest:
                print(f"Downloading {filename}", flush=True)
                data = download(f"{REPOSITORIES}/{self.name}/{filename}")
                if hashlib.sha256(data).hexdigest() != digest:
                    raise SystemExit(f"{filename} doesn't have the SHA-256 MSYS2's database gives it")
                self.cache.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            self.opened[package] = tarfile.open(fileobj=io.BytesIO(decompress(data)))

        return self.opened[package].extractfile(member).read()


def fields(text):
    """A pacman package description: its %FIELD%s' lines"""
    result, key = {}, None
    for line in text.splitlines():
        if line.startswith("%") and line.endswith("%"):
            key = line.strip("%")
            result[key] = []
        elif line and key:
            result[key].append(line)
    return result


def install(root):
    groups = folders(root)
    if not groups:
        raise SystemExit(f"No toolchain programs in {Path(root) / 'ee'}: extract ps2dev's release there, or set PS2DEV or "
                         "\"ps2dev\" in local.json to where it is")

    repositories = {}
    for folder in groups:
        added = []
        programs = sorted(folder.glob("*.exe"))
        machine, imports = pe_imports(programs[0])
        source = repositories.get(repository(machine, imports))
        if source is None:
            source = repositories[repository(machine, imports)] = Repository(repository(machine, imports),
                                                                             HERE / "build" / "msys2")

        pending = [name for program in programs for name in pe_imports(program)[1]]
        seen = set()
        while pending:
            name = pending.pop()
            if name in seen:
                continue

            seen.add(name)
            path = folder / name
            # Windows' own DLLs are no package's
            if name not in source.dlls:
                continue

            if not path.exists() or pe_imports(path)[0] != machine:
                path.write_bytes(source.dll(name))
                added.append(name)
            pending += pe_imports(path)[1]

        print(f"{folder}: {', '.join(sorted(added)) if added else 'nothing missing'}")


def system_has(name, machine):
    """Whether Windows has the DLL itself: an API set, or in its system folder for the machine (SysWOW64 for 32 bits)"""
    if name.startswith(("api-ms-", "ext-ms-")):
        return True

    windows = Path(os.environ.get("SystemRoot", "C:/Windows"))
    wow64 = windows / "SysWOW64"
    folder = wow64 if machine == I386 and wow64.is_dir() else windows / "System32"
    return (folder / name).exists()


def missing():
    """The DLLs Windows wouldn't find for the programs the build runs (next to them, its own, on the PATH), by program"""
    root = Path(local_config.ps2dev())
    programs = [Path(local_config.ee_tool(name)) for name in ("g++", "as", "ld", "nm")]
    programs += [root / "ee" / TARGET / "bin" / name for name in ("as.exe", "ld.exe")]
    programs += sorted((root / "ee" / "libexec" / "gcc" / TARGET).glob("*/cc1plus.exe"))
    path = [Path(folder) for folder in os.environ.get("PATH", "").split(os.pathsep) if folder]
    result = {}
    for program in programs:
        if not program.exists():
            continue

        machine, pending = pe_imports(program)
        seen = set()
        while pending:
            name = pending.pop()
            if name in seen or system_has(name, machine):
                continue

            seen.add(name)
            found = next((folder / name for folder in [program.parent] + path
                          if (folder / name).is_file() and pe_imports(folder / name)[0] == machine), None)
            if found is None:
                result.setdefault(name, []).append(program.name)
            else:
                pending += pe_imports(found)[1]

    return result


def check():
    """Stops with what to do when the build's programs couldn't start: no toolchain, or on Windows MSYS2's DLLs missing"""
    compiler = Path(local_config.ee_tool("g++"))
    if not compiler.exists():
        raise SystemExit(f"No toolchain: {compiler} isn't there. Extract ps2dev's release (see the README), or set PS2DEV or "
                         "\"ps2dev\" in local.json to where it is")

    if not local_config.WINDOWS:
        return

    absent = missing()
    if absent:
        names = ", ".join(sorted(absent))
        raise SystemExit(f"The toolchain's programs in {local_config.ps2dev()} can't start without MSYS2's {names}, which "
                         f"ps2dev's Windows release doesn't come with. Put {'them' if len(absent) > 1 else 'it'} next to the "
                         "programs with\n    .venv\\Scripts\\python.exe tools\\toolchain_dlls.py")


def main():
    install(local_config.ps2dev())


if __name__ == "__main__":
    main()
