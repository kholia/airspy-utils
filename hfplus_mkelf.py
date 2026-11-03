#!/usr/bin/env python3
#pylint: disable=line-too-long

"""
Wrap a raw Airspy HF+ firmware binary (hfplus-firmware-bb.bin or
hfplus-firmware-cd.bin) in a minimal ELF32 little-endian ARM container so
that analysis tools (IDA Pro, Ghidra, radare2, gdb) load it at the correct
flash address in Thumb mode.

Defaults match the ATSAM3U1 memory map: the image is linked at flash
address 0x80000 and the reset vector lives at 0x83309 (Thumb bit set).

Usage:

$ python3 hfplus_mkelf.py hfplus-firmware-cd.bin firmware.elf

$ python3 hfplus_mkelf.py hfplus-firmware-cd.bin firmware.elf 0x80000 0x83309
"""

import argparse
import struct
import sys

PY3 = sys.version_info.major == 3

EHDR_SIZE = 52
PHDR_SIZE = 32


def wrap(src_path, dst_path, base, entry):
    """
    Build a minimal ELF32 (EM_ARM, EABI5, one executable PT_LOAD).
    """

    data = open(src_path, "rb").read()

    payload_offset = EHDR_SIZE + PHDR_SIZE

    ehdr = b"\x7fELF\x01\x01\x01\x00" + b"\x00" * 8
    ehdr += struct.pack(
        "<HHIIIIIHHHHHH",
        2,            # e_type = ET_EXEC
        40,           # e_machine = EM_ARM
        1,            # e_version
        entry,        # e_entry
        EHDR_SIZE,    # e_phoff
        0,            # e_shoff
        0x05000200,   # e_flags: EABI5, soft-float
        EHDR_SIZE,    # e_ehsize
        PHDR_SIZE,    # e_phentsize
        1,            # e_phnum
        0, 0, 0)

    phdr = struct.pack(
        "<IIIIIIII",
        1,             # p_type = PT_LOAD
        payload_offset,  # p_offset
        base,          # p_vaddr
        base,          # p_paddr
        len(data),     # p_filesz
        len(data),     # p_memsz
        5,             # p_flags = R + X
        0x8000)        # p_align

    with open(dst_path, "wb") as f:
        f.write(ehdr + phdr + data)

    return len(data)


def main():
    """
    Driver code
    """

    if not PY3:
        print("This script requires Python 3.x to run!")
        sys.exit(1)

    parser = argparse.ArgumentParser()
    parser.add_argument('src', help="raw firmware .bin file")
    parser.add_argument('dst', help="output .elf file")
    parser.add_argument('base', nargs='?', default='0x80000', help="flash load address, default 0x80000 (ATSAM3U1)")
    parser.add_argument('entry', nargs='?', default='0x83309', help="reset vector address (Thumb), default 0x83309")
    args = parser.parse_args()

    base = int(args.base, 0)
    entry = int(args.entry, 0)

    size = wrap(args.src, args.dst, base, entry)
    print("Wrote %s (%d bytes of payload at 0x%X, entry 0x%X)" % (args.dst, size, base, entry))


if __name__ == "__main__":
    main()
