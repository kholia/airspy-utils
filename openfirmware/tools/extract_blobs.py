#!/usr/bin/env python3
"""
Extract the STA709 tuner register-table region from an official Airspy
HF+ firmware image, for use by the open firmware (roadmap phase 4).

The tuner bring-up/filter tables live in flash between the code and the
USB descriptor area. For R5.0.1-CD the known anchors (from the
decompilation of tuner_set_frequency / tuner_write_table, see
docs/airspy-hf-firmware-internals.md section 8) are:

    0x85848   filter table base (912k/456k/650k variants index into it)
    0x85A34   384 ksps filter table
    0x85AD4   228/192 ksps filter table
    0x86C43   end of the region (WinUSB descriptor follows at 0x86C48)

Output: a raw .bin blob plus a .h header with the anchors, ready to be
linked into the firmware as data.

Usage:

    python3 extract_blobs.py /path/to/hfplus-firmware-cd.bin out_dir
"""

import json
import os
import struct
import sys

REGION_START = 0x85848
REGION_END = 0x86C44          # exclusive
FLASH_BASE = 0x80000

ANCHORS = {
    "filters_912k_456k_650k_base": 0x85848,
    "filters_384k_base": 0x85A34,
    "filters_228k_192k_base": 0x85AD4,
}


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)

    src, out_dir = sys.argv[1], sys.argv[2]
    data = open(src, "rb").read()
    off_start = REGION_START - FLASH_BASE
    off_end = REGION_END - FLASH_BASE
    if len(data) < off_end:
        sys.exit("image too short - is this an official firmware .bin?")

    blob = data[off_start:off_end]
    os.makedirs(out_dir, exist_ok=True)

    with open(os.path.join(out_dir, "sta709_tables.bin"), "wb") as f:
        f.write(blob)

    with open(os.path.join(out_dir, "sta709_tables.h"), "w") as f:
        f.write("/*\n * STA709 register tables, extracted from the official %s\n"
                " * region 0x%X-0x%X. DATA ONLY - see openfirmware/README.md\n"
                " * for the licensing model of this directory.\n */\n\n"
                % (os.path.basename(src), REGION_START, REGION_END))
        f.write("#ifndef STA709_TABLES_H\n#define STA709_TABLES_H\n\n")
        f.write("#include <stdint.h>\n\n")
        for name, addr in ANCHORS.items():
            f.write("#define %-32s 0x%06X\n" % (name.upper(), addr))
        f.write("\nstatic const uint8_t sta709_tables[0x%X] = {\n    "
                % len(blob))
        f.write(",\n    ".join(",".join("0x%02X" % b
                for b in blob[i:i + 12]) for i in range(0, len(blob), 12)))
        f.write("\n};\n\n#endif /* STA709_TABLES_H */\n")

    manifest = {
        "source_image": os.path.basename(src),
        "region": [REGION_START, REGION_END],
        "anchors": ANCHORS,
        "size": len(blob),
        "note": "offsets are absolute flash addresses; subtract REGION_START for blob-relative offsets",
    }
    with open(os.path.join(out_dir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)

    print("wrote %d bytes of STA709 tables to %s" % (len(blob), out_dir))
    for name, addr in ANCHORS.items():
        print("  %-32s 0x%06X (blob offset 0x%X)"
              % (name, addr, addr - REGION_START))


if __name__ == "__main__":
    main()
