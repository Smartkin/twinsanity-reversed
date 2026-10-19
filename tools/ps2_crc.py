#!/usr/bin/env python3
"""Copies an ELF with a word appended that gives it a PS2 CRC: the XOR of the file's 32 bit words, which PCSX2 knows an
executable it boots by (its game settings, gamesettings/<CRC>.ini, and patches). TT Lab's development builds get the same CRC
whatever their code (configure.py's DEV_CRC), so PCSX2's settings for them stay one file. Loading an ELF only reads what its
headers point at: the word after the end changes nothing else.

    tools/ps2_crc.py <elf> <output> <crc>
"""
import shutil
import struct
import sys


def crc(data):
    value = 0
    for (word,) in struct.iter_unpack("<I", data[:len(data) // 4 * 4]):
        value ^= word
    return value


def main():
    source, output, wanted = sys.argv[1], sys.argv[2], int(sys.argv[3], 0)
    with open(source, "rb") as file:
        data = file.read()

    # Whole words, so the appended one is one of them
    data += bytes(-len(data) % 4)
    data += struct.pack("<I", crc(data) ^ wanted)
    with open(output, "wb") as file:
        file.write(data)

    shutil.copymode(source, output)


if __name__ == "__main__":
    main()
