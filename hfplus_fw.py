#!/usr/bin/env python3
#pylint: disable=line-too-long,anomalous-backslash-in-string

"""
Program to print the firmware version running on a Airspy HF+ device.
"""

import sys
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

    msg = "ATTENTION: The device is in 'bootloader' mode. Use the 'hfplus_reboot.py' script to switch the device to 'normal' mode."

    devs = usb.core.find(find_all=True)
    for dev in devs:
        if dev.idVendor == 0x03eb and dev.idProduct == 0x800c:
            return dev
    for dev in devs:
        if dev.idVendor == 0x03eb and dev.idProduct == 0x6124:
            print(msg)
            sys.exit(0)

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
    args = parser.parse_args()

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

    # Function prototype -> libusb_control_transfer (libusb_device_handle
    #   *dev_handle, uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue,
    #   uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int
    #   timeout)
    # Actual call -> libusb_control_transfer(dev, 0xc0, 9, 0, 0, buffer, 0x3f, 0);
    # ctrl_transfer(bmRequestType, bRequest, wValue=0, wIndex=0, data_or_wLength=None, timeout=None)
    msg = dev.ctrl_transfer(0xc0, 9, 0, 0, 0x3f).tobytes()
    omsg = []
    for element in msg:
        omsg.append(chr(element))
        if element == 0x00:
            break
    firmware_version = ''.join(omsg)
    print(firmware_version)

    # cleanup
    usb.util.dispose_resources(dev)


if __name__ == "__main__":
    main()
