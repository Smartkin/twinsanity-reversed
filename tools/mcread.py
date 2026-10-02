#!/usr/bin/env python3
"""Lists a PS2 memory card image's directories (PCSX2's .ps2 files: 528 byte pages of 512 bytes and their ECC), and dumps a
file's bytes. Read only.

    tools/mcread.py <card> [path]           the root, or the directory's entries (sizes, times, mode)
    tools/mcread.py <card> <file> --dump    the file's bytes to stdout
"""
import struct
import sys


class Card:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        magic = self.data[:28]
        if magic != b"Sony PS2 Memory Card Format ":
            raise SystemExit(f"{path} isn't a formatted PS2 memory card")
        (self.page_size, self.pages_per_cluster, self.pages_per_block, _, self.clusters,
         self.alloc_offset, self.alloc_end, self.root) = struct.unpack_from("<HHHHIIII", self.data, 0x28)
        self.ifc = struct.unpack_from("<32I", self.data, 0x50)
        self.raw_page = 528 if len(self.data) % 528 == 0 else 512
        self.cluster_size = self.page_size * self.pages_per_cluster

    def cluster(self, number):
        out = b""
        for page in range(self.pages_per_cluster):
            at = (number * self.pages_per_cluster + page) * self.raw_page
            out += self.data[at:at + self.page_size]
        return out

    def fat_entry(self, cluster):
        per = self.cluster_size // 4
        indirect = cluster // (per * per)
        fat_cluster = struct.unpack_from("<I", self.cluster(self.ifc[indirect]), (cluster // per % per) * 4)[0]
        return struct.unpack_from("<I", self.cluster(fat_cluster), cluster % per * 4)[0]

    def chain(self, first):
        cluster = first
        while True:
            yield cluster
            entry = self.fat_entry(cluster)
            if entry == 0xFFFFFFFF or not entry & 0x80000000:
                return
            cluster = entry & 0x7FFFFFFF

    def read(self, first, length):
        out = b""
        for cluster in self.chain(first):
            out += self.cluster(cluster + self.alloc_offset)
            if len(out) >= length:
                break
        return out[:length]

    def entries(self, first, count):
        raw = self.read(first, count * 512)
        for i in range(count):
            entry = raw[i * 512:(i + 1) * 512]
            mode, length = struct.unpack_from("<HxxI", entry, 0)
            created = entry[0x8:0x10]
            cluster = struct.unpack_from("<I", entry, 0x10)[0]
            modified = entry[0x18:0x20]
            name = entry[0x40:0x60].split(b"\0")[0].decode("latin-1")
            yield name, mode, length, cluster, created, modified

    def find(self, path):
        first, count = self.root, None
        root = next(self.entries(self.root, 1))
        count = root[2]
        for part in [part for part in path.split("/") if part]:
            for name, mode, length, cluster, *_ in self.entries(first, count):
                if name == part:
                    first, count, found_mode = cluster, length, mode
                    break
            else:
                raise SystemExit(f"no {part} in {path}")
        return first, count


def time_of(raw):
    _, second, minute, hour, day, month, year = struct.unpack("<BBBBBBH", raw)
    return f"{year:04}-{month:02}-{day:02} {hour:02}:{minute:02}:{second:02}"


def main():
    card = Card(sys.argv[1])
    path = sys.argv[2] if len(sys.argv) > 2 else "/"
    if "--dump" in sys.argv:
        directory, _, name = path.rpartition("/")
        first, count = card.find(directory)
        for entry_name, mode, length, cluster, *_ in card.entries(first, count):
            if entry_name == name:
                sys.stdout.buffer.write(card.read(cluster, length))
                return
        raise SystemExit(f"no {path}")

    first, count = card.find(path)
    for name, mode, length, cluster, created, modified in card.entries(first, count):
        kind = "dir " if mode & 0x20 else "file"
        print(f"{kind} {mode:04x} {length:8} {time_of(modified)} {name}")


main()
