#!/usr/bin/env python3
#pylint: disable=line-too-long,anomalous-backslash-in-string

"""
Program to change the 'operating mode' of Airspy HF+ devices.
"""

import os
import sys
import argparse

import usb.core
import usb.util

# $ bossac -i -p /dev/ttyACM0
# Device       : ATSAM3U1
# Version      : v1.2 Dec 16 2010 19:24:59
# Address      : 0x80000
# Pages        : 256
# Page Size    : 256 bytes
# Total Size   : 64KB
# Planes       : 1
# Lock Regions : 8
# Locked       : none
# Security     : false
# Boot Flash   : false


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
    parser.add_argument('-d', action='store_true', dest='debug_enabled', help="run in debugging mode")
    parser.add_argument('-b', action='store_true', dest='programming_mode_enabled', help="put device in bootloader/programming mode", default=False)
    parser.add_argument('-n', action='store_true', dest='normal_mode_enabled', help="put device in normal mode", default=False)
    parser.add_argument('-p', action='store', dest='device_path', help="device path, /dev/ttyACM0 if not specified", default="/dev/ttyACM0")
    args = parser.parse_args()

    if not args.programming_mode_enabled and not args.normal_mode_enabled:
        print("Please run this program with -p or the -n option. Use the -h option to get help.")
        sys.exit(1)

    if args.programming_mode_enabled:
        # find device
        dev = find_device()
        if dev is None:
            raise ValueError('Device not found!')

        # work with the device
        cfg = dev.get_active_configuration()
        if args.debug_enabled:
            print(cfg)
        dev.set_configuration(configuration=1)
        usb.util.claim_interface(dev, interface=0)
        dev.set_interface_altsetting(interface=0, alternate_setting=1)
        dev.clear_halt(ep=0x81)

        # Function prototype -> libusb_control_transfer (libusb_device_handle *dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout)
        # Actual call -> libusb_control_transfer(dev, 0x40, 0x66, 0xc0de, 0xbabe, 0, 0, 0);
        # ctrl_transfer(bmRequestType, bRequest, wValue=0, wIndex=0, data_or_wLength=None, timeout=None)
        try:
            dev.ctrl_transfer(0x40, 0x66, 0xc0de, 0xbabe, 0)
        except:
            pass  # should be ok?
        # Bus 003 Device 012: ID 03eb:6124 Atmel Corp. at91sam SAMBA bootloader
        print("Use the 'bossac' command to upload the new firmware! E.g 'sudo bossac -u -p /dev/ttyACM0; sudo bossac -e -b -v -p /dev/ttyACMO -w firmware.bin'.")

    elif args.normal_mode_enabled:
        os.system("bossac --boot=1 -p %s; bossac --reset -p %s" % (args.device_path, args.device_path))


if __name__ == "__main__":
    main()
