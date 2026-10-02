"""This machine's paths: the PS2SDK toolchain, the PAL disc image and PCSX2. Each comes from its environment variable, else
local.json next to configure.py (not shared, local.example.json shows its keys), else where it usually is on the system.

    PS2DEV / "ps2dev"         the toolchain's root (ee/bin/mips64r5900el-ps2-elf-*), /usr/local/ps2dev or C:/ps2dev
    PS2SDK / "ps2sdk"         PS2SDK, $PS2DEV/ps2sdk
    TWINSANITY_ISO / "disc_image"   the PAL disc image the game reads its files from
    PCSX2 / "pcsx2"           PCSX2's executable (or "flatpak" for the Flatpak), found when not given
"""
import json
import os
import shutil
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
WINDOWS = os.name == "nt"
EE_PREFIX = "ee/bin/mips64r5900el-ps2-elf-"


def _local():
    path = HERE / "local.json"
    if not path.exists():
        return {}

    with open(path, encoding="utf-8") as file:
        return json.load(file)


def _setting(variable, key, default=None):
    value = os.environ.get(variable) or _local().get(key)
    return value if value else default


def ps2dev():
    return Path(_setting("PS2DEV", "ps2dev", "C:/ps2dev" if WINDOWS else "/usr/local/ps2dev")).as_posix()


def ps2sdk():
    return Path(_setting("PS2SDK", "ps2sdk", f"{ps2dev()}/ps2sdk")).as_posix()


def ee_tool(name):
    """A tool of the EE's toolchain (as, g++, ld, nm), .exe on Windows"""
    return f"{ps2dev()}/{EE_PREFIX}{name}{'.exe' if WINDOWS else ''}"


def disc_image():
    value = _setting("TWINSANITY_ISO", "disc_image")
    return str(Path(value)) if value else None


def pcsx2():
    """How to start PCSX2: a list of arguments before its own ("flatpak run ... net.pcsx2.PCSX2", or the executable)"""
    value = _setting("PCSX2", "pcsx2")
    if value and value != "flatpak":
        return [value]

    if value == "flatpak" or (not WINDOWS and shutil.which("flatpak") and
                              os.system("flatpak info net.pcsx2.PCSX2 > /dev/null 2>&1") == 0):
        return ["flatpak"]

    for name in ("pcsx2-qt", "pcsx2"):
        found = shutil.which(name)
        if found:
            return [found]

    if WINDOWS:
        for root in (os.environ.get("ProgramFiles"), os.environ.get("ProgramFiles(x86)"),
                     os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs")):
            candidate = Path(root or "") / "PCSX2" / "pcsx2-qt.exe"
            if root and candidate.exists():
                return [str(candidate)]

    return None
