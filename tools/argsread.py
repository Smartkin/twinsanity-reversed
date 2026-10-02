#!/usr/bin/env python3
"""argsread.py FUNCTION...: which of $a1-$a3 a function may read before writing them, following its direct calls (a call passes
on what's still unwritten to what the callee reads; virtual calls and calls into unknown code read everything unwritten). Paths
are taken in file order (writes on any earlier line count as writes: approximate but conservative for straight-line prefixes)"""
import re, sys
from pathlib import Path
D = Path(__file__).resolve().parent.parent
REGS = ("$a1", "$a2", "$a3")
STORES = ("sw", "sh", "sb", "sd", "sq", "swc1", "sdc1", "sdl", "sdr", "swl", "swr")
BRANCHES = ("jr", "jalr", "beqz", "bnez", "beq", "bne", "beql", "bnel", "bgez", "bltz", "blez", "bgtz", "bgezl", "bltzl", "blezl",
            "bgtzl", "beqzl", "bnezl")
CACHE = {}

def lines(fn):
    path = D / f"asm/text/{fn}.s"
    if not path.exists():
        return None
    out = []
    for line in open(path):
        line = re.sub(r"^\s*/\* [0-9A-F]+ [0-9A-F]+ [0-9A-F]+ \*/\s*", "", line).strip()
        if line and not line.startswith((".", "glabel", "endlabel", "nonmatching", "/*", "jlabel")) and not re.match(r"\.L\w+:", line):
            out.append(re.sub(r"\s+", " ", line))
    return out

def reads(fn, stack=()):
    if fn in CACHE:
        return CACHE[fn]
    if fn in stack:
        return set(REGS)
    code = lines(fn)
    if code is None:
        # C++ (or unknown): its declaration decides; treat as reading nothing extra
        return set()
    written, read = set(), set()
    for i, line in enumerate(code):
        op, _, rest = line.partition(" ")
        args = [a.strip() for a in rest.split(",")] if rest else []
        if op == "jal":
            # the delay slot runs before the call
            if i + 1 < len(code):
                dop, _, drest = code[i + 1].partition(" ")
                dargs = [a.strip() for a in drest.split(",")] if drest else []
                if dargs and dop not in STORES and dop not in BRANCHES and dargs[0] in REGS:
                    for r in re.findall(r"\$\w+", ",".join(dargs[1:])):
                        if r in REGS and r not in written:
                            read.add(r)
                    written.add(dargs[0])
            callee = args[0]
            for r in reads(callee, stack + (fn,)):
                if r.rstrip("?") not in written:
                    read.add(r)
            written.update(REGS)
            continue
        if op == "jalr":
            # A virtual call: what it reads isn't known (marked, not counted as a direct read)
            for r in REGS:
                if r not in written:
                    read.add(r + "?")
            written.update(REGS)
            continue
        if op in STORES or op in BRANCHES:
            for r in re.findall(r"\$\w+", rest):
                if r in REGS and r not in written:
                    read.add(r)
            continue
        if args:
            for r in re.findall(r"\$\w+", ",".join(args[1:])):
                if r in REGS and r not in written:
                    read.add(r)
            if args[0] in REGS:
                written.add(args[0])
    CACHE[fn] = read
    return read

if __name__ == "__main__":
    for fn in sys.argv[1:]:
        print(fn, sorted(reads(fn)))
