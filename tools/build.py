#!/usr/bin/env python3
"""Builds build/SLES_525.68.elf the same way on Linux and Windows: configure.py first when build.ninja is missing or was
written on another system, then ninja (the one on the path, else the virtual environment's from requirements.txt).

    python tools/build.py [ninja arguments...]     build (the default target, or the ones given)
    python tools/build.py --matching               the asm alone must give the retail load image back (ninja check), then the
                                                   usual configuration again
    python tools/build.py --clean                  ninja -t clean
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


def configured_for(matching):
    path = HERE / "build.ninja"
    if not path.exists():
        return False

    text = path.read_text(encoding="utf-8", errors="replace")
    if not text.startswith(f"# host: {os.name}\n"):
        return False

    return ("configure.py --matching" in text) == matching


def configure(matching):
    subprocess.run(PYTHON + ["configure.py"] + (["--matching"] if matching else []), cwd=HERE, check=True)


def run_ninja(arguments):
    return subprocess.run(ninja() + arguments, cwd=HERE).returncode


def main():
    arguments = sys.argv[1:]
    if arguments[:1] == ["--clean"]:
        if not configured_for(False):
            configure(False)
        sys.exit(run_ninja(["-t", "clean"]))

    toolchain_dlls.check()
    if arguments[:1] == ["--matching"]:
        configure(True)
        try:
            code = run_ninja(["check"] + arguments[1:])
        finally:
            configure(False)
        sys.exit(code)

    if not configured_for(False):
        configure(False)

    sys.exit(run_ninja(arguments))


if __name__ == "__main__":
    main()
