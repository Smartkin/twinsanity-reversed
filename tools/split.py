#!/usr/bin/env python3
"""Splits the retail executable into asm/ and assets/ again from symbol_addrs.txt, after the names changed. Twice: the second time
every function spimdisasm found inside another's file gets a file of its own too (make_splat.py --inner). Then configure.py.

    .venv/bin/python tools/split.py          (Windows: .venv\\Scripts\\python tools\\split.py)

Run it with the virtual environment's Python, the one that has splat (requirements.txt).
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
PYTHON = [sys.executable, "-X", "utf8"]


def run(arguments, log=None):
    if log is None:
        result = subprocess.run(PYTHON + arguments, cwd=HERE)
        if result.returncode != 0:
            sys.exit(result.returncode)
        return

    with open(log, "w", encoding="utf-8") as output:
        result = subprocess.run(PYTHON + arguments, cwd=HERE, stdout=output, stderr=subprocess.STDOUT)

    if result.returncode != 0:
        print("".join(log.read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)[-20:]))
        sys.exit(result.returncode)


def clear():
    for folder in ("asm", "assets"):
        shutil.rmtree(HERE / folder, ignore_errors=True)


def split():
    clear()
    run(["-m", "splat", "split", "build/splat.yaml"], HERE / "build" / "split.log")
    if os.name == "nt":
        # splat writes the system's line ends; the asm stays the same on every system
        for path in (HERE / "asm").rglob("*.s"):
            data = path.read_bytes()
            if b"\r\n" in data:
                path.write_bytes(data.replace(b"\r\n", b"\n"))


def main():
    if not (HERE / "SLES_525.68").exists():
        raise SystemExit("SLES_525.68 (the PAL disc's executable) has to be next to configure.py")

    try:
        import splat  # noqa: F401
    except ImportError:
        raise SystemExit(f"{sys.executable} has no splat: run this with the virtual environment's Python (requirements.txt)")

    (HERE / "build").mkdir(exist_ok=True)
    run(["tools/make_splat.py"])
    split()
    run(["tools/make_splat.py", "--inner"])
    split()
    run(["tools/fix_asm.py"])
    run(["configure.py"])


if __name__ == "__main__":
    main()
