#!/usr/bin/env python3
"""
Read-only probe suite for an Airspy HF+ device (normal mode, 03eb:800c).

Reproduces the [live] markers in docs/airspy-hf-firmware-internals.md:
firmware version, samplerate table, architectures, filter gain, serial
number, config page, the stall behaviour of the dead commands (16-22), the
undocumented commands (100-105) and the reboot discriminator.

SAFETY:
 - every request below is a read or a verified no-op on this firmware
   family;
 - the reboot test deliberately sends WRONG magic (0x1111/0x2222); the
   decompiled handler at 0x835F8 only acts on an exact 0xC0DE/0xBABE
   match. Do not "fix" these values unless you want to land in the
   SAM-BA bootloader (03eb:6124; recover with hfplus_reboot.py -n).

Requires: pyusb (pip install pyusb).
"""

import sys
import usb.core
import usb.util

VID, PID = 0x03eb, 0x800c


def find_device():
    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        # helpful hint: bootloader mode?
        if usb.core.find(idVendor=VID, idProduct=0x6124) is not None:
            sys.exit("device is in SAM-BA bootloader mode; run hfplus_reboot.py -n first")
        sys.exit("Airspy HF+ not found (normal mode 03eb:800c)")
    return dev


def vin(dev, req, val=0, idx=0, ln=64):
    """Vendor IN request; returns 'ACK <n> <hex>' or 'STALL'."""
    try:
        data = dev.ctrl_transfer(0xC0, req, val, idx, ln, timeout=500).tobytes()
        return "ACK %3d %s" % (len(data), data.hex())
    except usb.core.USBError:
        return "STALL"


def vout(dev, req, val=0, idx=0):
    """Vendor OUT request with no data phase; returns 'ACK' or 'STALL'."""
    try:
        dev.ctrl_transfer(0x40, req, val, idx, b"", timeout=500)
        return "ACK"
    except usb.core.USBError:
        return "STALL"


def main():
    dev = find_device()
    dev.set_configuration(1)
    usb.util.claim_interface(dev, 0)
    dev.set_interface_altsetting(0, 1)
    try:
        dev.clear_halt(0x81)
    except usb.core.USBError:
        pass

    try:
        print("== 9 GET_VERSION_STRING ==")
        r = dev.ctrl_transfer(0xC0, 9, 0, 0, 63, timeout=500).tobytes()
        print(r.split(b"\x00")[0].decode(errors="replace"))

        print("\n== 3 GET_SAMPLERATES (wIndex=0 -> count, wIndex=7 -> array) ==")
        print(vin(dev, 3, idx=0, ln=4))
        print(vin(dev, 3, idx=7, ln=28))

        print("\n== 14 GET_SAMPLERATE_ARCHITECTURES ==")
        print(vin(dev, 14, idx=0, ln=8))
        print(vin(dev, 14, idx=7, ln=7))

        print("\n== 15 GET_FILTER_GAIN ==")
        print(vin(dev, 15, ln=1))

        print("\n== 7 GET_SERIALNO_BOARDID (20 bytes) ==")
        print(vin(dev, 7, ln=20))

        print("\n== 5 CONFIG_READ (wLength must be 256; showing first 32 bytes) ==")
        print(vin(dev, 5, ln=256)[:14 + 64])

        print("\n== dead commands from airspyhf_commands.h (expect STALL) ==")
        for c in (16, 17, 18, 19, 20, 21, 22):
            print("case %2d: %s" % (c, vin(dev, c)))

        print("\n== undocumented commands ==")
        for c in (48, 100, 101, 103, 104, 105):
            print("case %3d: %s" % (c, vin(dev, c)))

        print("\n== 102 reboot discriminator, WRONG magic (safe no-op) ==")
        print("case 102: %s" % vout(dev, 102, 0x1111, 0x2222))
    finally:
        usb.util.dispose_resources(dev)


if __name__ == "__main__":
    main()
