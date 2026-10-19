#!/usr/bin/env python3
"""Makes the folder a TT Lab development build plays from (docs/DEVELOPMENT.md, "TT Lab's development builds"), the way TT Lab's
Play makes its own: every file of the disc's archive (Crash6\\Crash.BH and BD) on its own, named in upper case as the game asks
for it (LEVELS/EARTH/HUB/BEACH.RM2), and the sound banks next to the archive (CRASH6/MUSIC.MB), which the game streams through the
same loader. --elf copies the development build in as SLES_525.68 and --start-chunk writes STARTCHUNK.TXT, the chunk it starts
in. The disc is an extracted disc folder or the disc image itself. A file the folder has at the same size is kept.

    tools/dev_folder.py <disc folder or image> <folder> [--elf build/tt-lab-dev/SLES_525.68.elf] [--start-chunk PATH]

Then: tools/run_pcsx2.py <folder>/SLES_525.68 <disc image> --map build/tt-lab-dev/SLES_525.68.map --host-fs [--quick]
"""
import argparse
import os
import shutil
import struct
from pathlib import Path

SECTOR = 2048


class DiscImage:
    """An ISO 9660 image's files by path (the PS2 discs' file system): the primary volume descriptor's root directory, walked"""

    def __init__(self, path):
        self.file = open(path, "rb")
        descriptor = self.read(16 * SECTOR, SECTOR)
        if descriptor[1:6] != b"CD001":
            raise SystemExit(f"{path} isn't a disc image (no ISO 9660 volume)")
        self.root = self.record(descriptor, 156)

    def read(self, offset, size):
        self.file.seek(offset)
        return self.file.read(size)

    @staticmethod
    def record(data, at):
        """A directory record: (name, first sector, size, whether it's a directory)"""
        sector, size = struct.unpack_from("<I", data, at + 2)[0], struct.unpack_from("<I", data, at + 10)[0]
        name = data[at + 33:at + 33 + data[at + 32]].decode("latin-1").split(";")[0]
        return name, sector, size, data[at + 25] & 2 != 0

    def entries(self, directory):
        _, sector, size, _ = directory
        data = self.read(sector * SECTOR, size)
        at = 0
        while at < len(data):
            length = data[at]
            if length == 0:
                # Records don't cross sectors: the rest of this one is empty
                at = (at // SECTOR + 1) * SECTOR
                continue
            entry = self.record(data, at)
            if entry[0] not in ("\0", "\1"):
                yield entry
            at += length

    def find(self, *names):
        current = self.root
        for name in names:
            current = next((entry for entry in self.entries(current) if entry[0].upper() == name.upper()), None)
            if current is None:
                return None
        return current

    def files(self, *names):
        folder = self.find(*names)
        return [entry for entry in self.entries(folder) if not entry[3]] if folder else []

    def contents(self, entry):
        return self.read(entry[1] * SECTOR, entry[2])

    def copy(self, entry, destination, offset=0, size=None):
        _, sector, length, _ = entry
        remaining = length - offset if size is None else size
        self.file.seek(sector * SECTOR + offset)
        with open(destination, "wb") as out:
            while remaining > 0:
                block = self.file.read(min(remaining, 1 << 20))
                out.write(block)
                remaining -= len(block)


class DiscFolder:
    """An extracted disc's files by path, whatever their case"""

    def __init__(self, path):
        self.root = Path(path)

    def find(self, *names):
        current = self.root
        for name in names:
            found = [entry for entry in current.iterdir() if entry.name.upper() == name.upper()] if current.is_dir() else []
            if not found:
                return None
            current = found[0]
        return current

    def files(self, *names):
        folder = self.find(*names)
        return [entry for entry in folder.iterdir() if entry.is_file()] if folder else []

    @staticmethod
    def contents(entry):
        return entry.read_bytes()

    @staticmethod
    def copy(entry, destination, offset=0, size=None):
        with open(entry, "rb") as source, open(destination, "wb") as out:
            source.seek(offset)
            remaining = entry.stat().st_size - offset if size is None else size
            while remaining > 0:
                block = source.read(min(remaining, 1 << 20))
                out.write(block)
                remaining -= len(block)


def name_of(entry):
    return entry[0] if isinstance(entry, tuple) else entry.name


def size_of(entry):
    return entry[2] if isinstance(entry, tuple) else entry.stat().st_size


def archive_records(data):
    """Crash.BH: a word, then each file's path (its length first), its offset in Crash.BD and its size"""
    records, at = [], 4
    while at < len(data):
        length, = struct.unpack_from("<i", data, at)
        path = data[at + 4:at + 4 + length].decode("latin-1")
        offset, size = struct.unpack_from("<ii", data, at + 4 + length)
        records.append((path, offset, size))
        at += 12 + length
    return records


def write(disc, entry, destination, offset=0, size=None):
    """The file (a part of it) into the folder, unless it's there at that size already. Whether it was written"""
    size = size_of(entry) - offset if size is None else size
    if destination.exists() and destination.stat().st_size == size:
        return False
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + ".tmp")
    disc.copy(entry, temporary, offset, size)
    os.replace(temporary, destination)
    return True


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("disc", help="an extracted disc folder or the disc image")
    parser.add_argument("folder")
    parser.add_argument("--elf", help="the development build, copied in as SLES_525.68")
    parser.add_argument("--start-chunk", help="the chunk to start in, written into STARTCHUNK.TXT (levels\\earth\\hub\\beach)")
    args = parser.parse_args()

    disc = DiscImage(args.disc) if Path(args.disc).is_file() else DiscFolder(args.disc)
    header, data = disc.find("Crash6", "Crash.BH"), disc.find("Crash6", "Crash.BD")
    if header is None or data is None:
        raise SystemExit(f"{args.disc} has no Crash6/Crash.BH and Crash.BD")

    folder = Path(args.folder)
    folder.mkdir(parents=True, exist_ok=True)
    records = archive_records(disc.contents(header))
    written = sum(write(disc, data, folder / path.replace("\\", "/").upper(), offset, size) for path, offset, size in records)
    banks = [entry for entry in disc.files("Crash6") if Path(name_of(entry)).suffix.upper() in (".MH", ".MB")]
    written += sum(write(disc, entry, folder / "CRASH6" / name_of(entry).upper()) for entry in banks)
    print(f"{folder}: {len(records)} files of the archive and {len(banks)} sound banks, {written} written")

    if args.elf:
        shutil.copy2(args.elf, folder / "SLES_525.68")
    if args.start_chunk is not None:
        (folder / "STARTCHUNK.TXT").write_text(args.start_chunk + "\n", encoding="latin-1", newline="\n")


if __name__ == "__main__":
    main()
