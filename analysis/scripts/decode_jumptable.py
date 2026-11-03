#!/usr/bin/env python3
"""
Decode the vendor-request jump table of the Airspy HF+ firmware.

The dispatcher at 0x8527C switches over bRequest via a table of 106
(0x6A) thumb-entries at 0x852A8 (R5.0.1-CD, see
docs/airspy-hf-firmware-internals.md section 4). This script prints the
case -> handler map straight from the raw binary, so it also works as a
template for other tables/other firmware revisions.

Usage:

    python3 decode_jumptable.py hfplus-firmware-cd.bin
    python3 decode_jumptable.py hfplus-firmware-bb.bin 0x852A8 106

Requires: nothing beyond Python 3 (capstone optional for extra decoding).
"""

import argparse
import struct
import sys

# R5.0.1-CD defaults
DEFAULT_TABLE_ADDR = 0x852A8
DEFAULT_ENTRIES = 106
FLASH_BASE = 0x80000

KNOWN = {
    0x8528A: "case 0 (no-op)",
    0x8557A: "case 1 RECEIVER_MODE",
    0x8549A: "case 2 SET_FREQ (OUT callback)",
    0x854AC: "case 3 GET_SAMPLERATES",
    0x854CC: "case 4 SET_SAMPLERATE",
    0x854DE: "case 5 CONFIG_READ",
    0x8547A: "case 6 CONFIG_WRITE (OUT callback)",
    0x854C0: "case 7 GET_SERIALNO_BOARDID",
    0x855C0: "case 8 SET_USER_OUTPUT",
    0x855CE: "case 9 GET_VERSION_STRING",
    0x855E4: "case 10 SET_AGC",
    0x85608: "case 11 SET_AGC_THRESHOLD",
    0x855F0: "case 12 SET_ATT",
    0x855FC: "case 13 SET_LNA",
    0x85466: "case 14 GET_SAMPLERATE_ARCHITECTURES",
    0x85454: "case 15 GET_FILTER_GAIN",
    0x85450: "DEFAULT STALL STUB (16-99 except 48)",
    0x85546: "case 48 WinUSB descriptor (wIndex must be 4)",
    0x85560: "case 100 diagnostics readout",
    0x854F4: "case 101 flash window read",
    0x8550A: "case 102 REBOOT TO SAM-BA (magic C0DE/BABE)",
    0x85514: "case 103 SET_FRONTEND_OPTIONS (wValue|wIndex<<16)",
    0x85530: "case 104 status query (6 bytes)",
    0x855A4: "case 105 string query",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("firmware", help="raw firmware .bin (e.g. hfplus-firmware-cd.bin)")
    parser.add_argument("table_addr", nargs="?", type=lambda x: int(x, 0),
                        default=DEFAULT_TABLE_ADDR, help="table address in flash (default 0x852A8)")
    parser.add_argument("entries", nargs="?", type=int, default=DEFAULT_ENTRIES,
                        help="number of entries (default 106)")
    args = parser.parse_args()

    data = open(args.firmware, "rb").read()
    off = args.table_addr - FLASH_BASE
    if off < 0 or off + 4 * args.entries > len(data):
        sys.exit("table 0x%X is outside this image" % args.table_addr)

    words = struct.unpack("<%dI" % args.entries, data[off:off + 4 * args.entries])
    prev = None
    for i, w in enumerate(words):
        target = w & ~1
        note = KNOWN.get(target, "")
        if target == prev and not note:
            print("case %3d (0x%02X): 0x%05X  (= previous)" % (i, i, target))
        else:
            print("case %3d (0x%02X): 0x%05X  %s" % (i, i, target, note))
        prev = target


if __name__ == "__main__":
    main()
