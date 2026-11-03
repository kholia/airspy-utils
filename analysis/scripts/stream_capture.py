#!/usr/bin/env python3
"""
Capture a short burst of the Airspy HF+ raw IQ stream and print simple
statistics, then stop the receiver.

Wire format (docs/airspy-hf-firmware-internals.md section 5.2): bulk IN
endpoint 0x81, 16 KB per transfer = 4096 interleaved complex int16 LE
samples, memory layout { int16 im; int16 re; } - imaginary first.

Requires: pyusb (pip install pyusb).
"""

import argparse
import struct
import sys
import usb.core
import usb.util

VID, PID = 0x03eb, 0x800c
TRANSFER_SIZE = 16384  # 4096 complex int16 samples
XFER_COUNT = 12


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-f", "--freq-khz", type=int, default=7200, help="tune frequency in kHz (default 7200)")
    parser.add_argument("-n", "--count", type=int, default=XFER_COUNT, help="number of 16 KB transfers (default 12)")
    parser.add_argument("-o", "--output", help="optional raw file to append the captures to")
    args = parser.parse_args()

    dev = usb.core.find(idVendor=VID, idProduct=PID)
    if dev is None:
        sys.exit("Airspy HF+ not found")

    dev.set_configuration(1)
    usb.util.claim_interface(dev, 0)
    dev.set_interface_altsetting(0, 1)
    try:
        dev.clear_halt(0x81)
    except usb.core.USBError:
        pass

    outfile = open(args.output, "ab") if args.output else None
    try:
        # 2 = SET_FREQ, 4-byte big-endian kHz in the control-OUT data phase
        dev.ctrl_transfer(0x40, 2, 0, 0, struct.pack(">I", args.freq_khz), timeout=500)
        # 1 = RECEIVER_MODE, 1 = on
        dev.ctrl_transfer(0x40, 1, 1, 0, 0, timeout=500)

        try:
            for i in range(args.count):
                buf = bytes(dev.read(0x81, TRANSFER_SIZE, timeout=3000))
                s = struct.unpack("<%dh" % (len(buf) // 2), buf)
                rms = (sum(x * x for x in s) / len(s)) ** 0.5
                print("transfer %2d: %d bytes, min %6d, max %6d, rms %7.1f"
                      % (i, len(buf), min(s), max(s), rms))
                if outfile:
                    outfile.write(buf)
        finally:
            # receiver OFF even if the capture loop breaks
            dev.ctrl_transfer(0x40, 1, 0, 0, 0, timeout=500)
            print("receiver off")
    finally:
        if outfile:
            outfile.close()
        usb.util.dispose_resources(dev)
        print("released device")


if __name__ == "__main__":
    main()
