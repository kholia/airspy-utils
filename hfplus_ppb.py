#!/usr/bin/env python3
#pylint: disable=line-too-long,anomalous-backslash-in-string

"""
Program to print / save the calibration (ppb) value for Airspy HF+ devices.
"""

import sys
import struct
import argparse

import usb.core
import usb.util

PY3 = sys.version_info.major == 3


def find_device():
    """
    Detect Airspy HF+ Discovery.

    $ lsusb
    Bus 003 Device 011: ID 03eb:800c Atmel Corp. Airspy HF+
    """

    devs = usb.core.find(find_all=True)
    for dev in devs:
        if dev.idVendor == 0x03eb and dev.idProduct == 0x800c:
            return dev

    return None


def main():
    """
    Driver code
    """

    if not PY3:
        print("This script requires Python 3.x to run!")
        sys.exit(1)

    parser = argparse.ArgumentParser()
    parser.add_argument('-v', action='store_true', dest='verbose_enabled', help="run in verbose mode", default=False)
    parser.add_argument('-d', action='store_true', dest='dump', help="dump full 256-bytes, redirect to a file please", default=False)
    parser.add_argument('-r', action='store_true', dest='read_ppb', help="read ppb value", default=False)
    parser.add_argument('-w', action='store_true', dest='write_ppb', help="write ppb value", default=False)
    parser.add_argument('-p', action='store', type=int, dest='ppb', help="ppb value (between 2500 to -2500), default is 0", default=0)
    args = parser.parse_args()

    # find device
    dev = find_device()
    if dev is None:
        raise ValueError('Device not found!')

    # work with the device
    cfg = dev.get_active_configuration()
    if args.verbose_enabled:
        print(cfg)
    dev.set_configuration(configuration=1)
    usb.util.claim_interface(dev, interface=0)
    dev.set_interface_altsetting(interface=0, alternate_setting=1)
    dev.clear_halt(ep=0x81)

    # Function prototype -> libusb_control_transfer (libusb_device_handle *dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout)
    # Actual call -> libusb_control_transfer(dev, 0xc0, 5, 0, 0, buffer, 0x100, 0);
    # ctrl_transfer(bmRequestType, bRequest, wValue=0, wIndex=0, data_or_wLength=None, timeout=None)
    if args.dump or args.read_ppb:
        msg = dev.ctrl_transfer(0xc0, 5, 0, 0, 0x100).tobytes()
        if args.dump:
            sys.stdout.buffer.write(msg)
        if args.read_ppb:
            ppb = msg[4:4+2]
            ppb = struct.unpack("<H", ppb)[0]
            print(ppb)

    # Actual call -> libusb_control_transfer(dev, 0x40, 6, 0, 0, buffer_to_write_to_device, 0x100, 0);
    if args.write_ppb:
        data = bytearray(4)  # some magic string
        data[0] = 0xb0
        data[1] = 0x71
        data[2] = 0xca
        data[3] = 0xa5
        data = data + struct.pack("<H", args.ppb)
        data = data + bytearray(250)
        dev.ctrl_transfer(0x40, 6, 0, 0, data)
        print("OK!")

    # cleanup
    usb.util.dispose_resources(dev)


if __name__ == "__main__":
    main()
