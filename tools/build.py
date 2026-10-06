#!/usr/bin/env python3
"""Builds build/SLES_525.68.elf the same way on Linux and Windows: configure.py first when build.ninja is missing or was
written on another system, then ninja (the one on the path, else the virtual environment's from requirements.txt).

    python tools/build.py [ninja arguments...]     build (the default target, or the ones given)
    python tools/build.py --clean                  ninja -t clean
    python tools/build.py --platform desktop ...   the same for the desktop's build/desktop/twinsanity (build/desktop/build.ninja)
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

import toolchain_dlls

HERE = Path(__file__).resolve().parent.parent
PYTHON = [sys.executable, "-X", "utf8"]


def ninja():
    found = shutil.which("ninja")
    if found:
        return [found]

    beside = Path(sys.executable).parent / ("ninja.exe" if os.name == "nt" else "ninja")
    if beside.exists():
        return [str(beside)]

    try:
        import ninja as module  # noqa: F401
        return PYTHON + ["-m", "ninja"]
    except ImportError:
        raise SystemExit("No ninja: install it (pip install -r requirements.txt puts one in .venv) or put it on the path")


PLATFORM = "ps2"
NINJA_FILE = "build.ninja"


def configured():
    path = HERE / NINJA_FILE
    if not path.exists():
        return False

    return path.read_text(encoding="utf-8", errors="replace").startswith(f"# host: {os.name}\n")


def configure():
    subprocess.run(PYTHON + ["configure.py", "--platform", PLATFORM], cwd=HERE, check=True)


def run_ninja(arguments):
    return subprocess.run(ninja() + ["-f", NINJA_FILE] + arguments, cwd=HERE).returncode


def main():
    global PLATFORM, NINJA_FILE
    arguments = sys.argv[1:]
    if arguments[:1] == ["--platform"]:
        PLATFORM = arguments[1]
        arguments = arguments[2:]
        if PLATFORM == "desktop":
            NINJA_FILE = "build/desktop/build.ninja"

    if arguments[:1] == ["--clean"]:
        if not configured():
            configure()
        sys.exit(run_ninja(["-t", "clean"]))

    if PLATFORM == "ps2":
        toolchain_dlls.check()
    if not configured():
        configure()

    sys.exit(run_ninja(arguments))


if __name__ == "__main__":
    main()
