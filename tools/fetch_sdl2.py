#!/usr/bin/env python3
"""Fetches SDL2 for the desktop build on Windows (or built for Windows with MinGW elsewhere): SDL2's MinGW development release,
checked against its SHA-256, of which the 32 bit folder (i686-w64-mingw32: bin/SDL2.dll, include/SDL2, lib) and the license go
to build/sdl2/, where configure.py finds it when nothing else gives an SDL2. The release stays there, a second run only unpacks
it again.

    python tools/fetch_sdl2.py
"""
import hashlib
import shutil
import tarfile
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
VERSION = "2.32.10"
ARCHIVE = f"SDL2-devel-{VERSION}-mingw.tar.gz"
URL = f"https://github.com/libsdl-org/SDL/releases/download/release-{VERSION}/{ARCHIVE}"
SHA256 = "83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2"
TARGET = "i686-w64-mingw32"
OUT = HERE / "build" / "sdl2"


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    archive = OUT / ARCHIVE
    if not archive.exists() or hashlib.sha256(archive.read_bytes()).hexdigest() != SHA256:
        print(f"Downloading {URL}")
        with urllib.request.urlopen(URL, timeout=120) as response:
            data = response.read()

        if hashlib.sha256(data).hexdigest() != SHA256:
            raise SystemExit(f"{ARCHIVE} isn't the release's: its SHA-256 differs")

        archive.write_bytes(data)

    shutil.rmtree(OUT / TARGET, ignore_errors=True)
    top = f"SDL2-{VERSION}/"
    with tarfile.open(archive) as release:
        members = [member for member in release.getmembers()
                   if member.name.startswith(f"{top}{TARGET}/") or member.name == f"{top}LICENSE.txt"]
        for member in members:
            member.name = member.name[len(top):]

        release.extractall(OUT, members=members, filter="data")

    print(f"SDL2 {VERSION} for 32 bit Windows: {(OUT / TARGET).as_posix()}")


if __name__ == "__main__":
    main()
