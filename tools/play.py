#!/usr/bin/env python3
"""Plays the build in PCSX2: build/SLES_525.68.elf booted (fast boot) with the PAL disc image, the way the game reads its files.
PCSX2 and the disc image come from tools/local_config.py (local.json, $PCSX2, $TWINSANITY_ISO; PCSX2 is found when not
given: the Flatpak, pcsx2-qt on the path, its install in Program Files). It plays with PCSX2's own settings and memory cards.
The script waits for PCSX2 and closes it when it's stopped itself (VS Code's Stop).

    python tools/play.py [--elf build/SLES_525.68.elf] [--iso IMAGE] [--pcsx2 EXECUTABLE]
    python tools/play.py --boot-test [run_pcsx2.py's options...]     (Linux) the boot test: tools/run_pcsx2.py on the build
"""
import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "tools"))
import local_config

FLATPAK_ID = "net.pcsx2.PCSX2"


def descendants(pid):
    """Linux: every process under pid (the Flatpak's sandbox and PCSX2 in it)"""
    children = {}
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        try:
            with open(f"/proc/{entry}/stat") as file:
                parent = int(file.read().rsplit(")", 1)[1].split()[1])
        except (OSError, IndexError, ValueError):
            continue
        children.setdefault(parent, []).append(int(entry))

    found, pending = [], [pid]
    while pending:
        for child in children.get(pending.pop(), []):
            found.append(child)
            pending.append(child)

    return found


def stop(process):
    if process.poll() is not None:
        return

    if os.name == "nt":
        process.terminate()
        return

    for pid in reversed(descendants(process.pid)):
        try:
            os.kill(pid, signal.SIGTERM)
        except OSError:
            pass

    process.terminate()


def die_with_this_process():
    """Linux: the launcher gets SIGTERM when this script dies, even killed (a debugger's Stop)"""
    try:
        import ctypes
        ctypes.CDLL(None, use_errno=True).prctl(1, signal.SIGTERM)  # PR_SET_PDEATHSIG
    except (OSError, AttributeError):
        pass


def tie_to_this_process(process):
    """Windows: PCSX2 in a job that closes with this script's last handle, so stopping the script stops the game"""
    import ctypes
    from ctypes import wintypes

    class BasicLimits(ctypes.Structure):
        _fields_ = [("PerProcessUserTimeLimit", ctypes.c_int64), ("PerJobUserTimeLimit", ctypes.c_int64),
                    ("LimitFlags", wintypes.DWORD), ("MinimumWorkingSetSize", ctypes.c_size_t),
                    ("MaximumWorkingSetSize", ctypes.c_size_t), ("ActiveProcessLimit", wintypes.DWORD),
                    ("Affinity", ctypes.c_size_t), ("PriorityClass", wintypes.DWORD), ("SchedulingClass", wintypes.DWORD)]

    class IoCounters(ctypes.Structure):
        _fields_ = [(name, ctypes.c_uint64) for name in ("ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                                                        "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

    class ExtendedLimits(ctypes.Structure):
        _fields_ = [("BasicLimitInformation", BasicLimits), ("IoInfo", IoCounters), ("ProcessMemoryLimit", ctypes.c_size_t),
                    ("JobMemoryLimit", ctypes.c_size_t), ("PeakProcessMemoryUsed", ctypes.c_size_t),
                    ("PeakJobMemoryUsed", ctypes.c_size_t)]

    kill_on_job_close = 0x2000
    extended_limit_information = 9
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.CreateJobObjectW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR]
    kernel32.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
    kernel32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    job = kernel32.CreateJobObjectW(None, None)
    limits = ExtendedLimits()
    limits.BasicLimitInformation.LimitFlags = kill_on_job_close
    if not job or not kernel32.SetInformationJobObject(job, extended_limit_information, ctypes.byref(limits),
                                                       ctypes.sizeof(limits)):
        return None

    kernel32.AssignProcessToJobObject(job, int(process._handle))
    return job


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--elf", default=str(HERE / "build" / "SLES_525.68.elf"))
    parser.add_argument("--iso", default=local_config.disc_image())
    parser.add_argument("--pcsx2")
    parser.add_argument("--boot-test", action="store_true")
    args, rest = parser.parse_known_args()

    elf = Path(args.elf).resolve()
    if not elf.exists():
        raise SystemExit(f"{elf} isn't built yet: python tools/build.py")
    if not args.iso or not Path(args.iso).exists():
        raise SystemExit("The PAL disc image isn't set or isn't there: put its path in local.json (\"disc_image\", see "
                         "local.example.json) or $TWINSANITY_ISO, or give --iso")
    iso = Path(args.iso).resolve()

    if args.boot_test:
        if os.name == "nt":
            raise SystemExit("The boot test (tools/run_pcsx2.py) drives PCSX2's Flatpak and runs on Linux")
        sys.exit(subprocess.run([sys.executable, str(HERE / "tools" / "run_pcsx2.py"), str(elf), str(iso), "--map",
                                 str(elf.with_suffix(".map"))] + rest).returncode)

    launcher = [args.pcsx2] if args.pcsx2 else local_config.pcsx2()
    if not launcher:
        raise SystemExit("PCSX2 wasn't found: put its executable's path in local.json (\"pcsx2\", see local.example.json) or "
                         "$PCSX2, or give --pcsx2")

    if launcher == ["flatpak"]:
        launcher = ["flatpak", "run", f"--filesystem={elf.parent}:ro", f"--filesystem={iso.parent}:ro", FLATPAK_ID]

    command = launcher + ["-fastboot", "-elf", str(elf)] + rest + ["--", str(iso)]
    print(" ".join(f'"{part}"' if " " in part else part for part in command), flush=True)
    process = subprocess.Popen(command, preexec_fn=None if os.name == "nt" else die_with_this_process)
    job = None
    if os.name == "nt":
        try:
            job = tie_to_this_process(process)
        except (OSError, AttributeError):
            job = None
    else:
        signal.signal(signal.SIGTERM, lambda *_: (stop(process), sys.exit(0)))
        signal.signal(signal.SIGHUP, lambda *_: (stop(process), sys.exit(0)))

    try:
        while process.poll() is None:
            time.sleep(0.5)
    except KeyboardInterrupt:
        stop(process)

    del job
    sys.exit(process.returncode or 0)


if __name__ == "__main__":
    main()
