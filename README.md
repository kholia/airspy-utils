### Airspy-Utils

`Airspy-Utils` is a small software collection to help with firmware related
operations on Airspy HF+ devices on Linux (and other free systems).

Why? For 'best results' it is often recommended to deploy the latest firmware.
This small software utility collection helps Linux users with deploying the
latest Airspy firmware.

UPDATE: The sole purpose of this work is to help out Linux users (like myself).
I am NOT interested in starting licensing debates / controversies related to
the upstream Airspy project. I am a just a new Airspy Linux customer who has
zero knowledge about DSP (none is required for this work), and Airspy's
previous history, behaviour, and origin.


#### Currently Supported Devices

- Airspy HF+ Discovery

This is the only Airspy device I have. Hint: Sponsorships can change this ;)


#### Software Requirements

- Python 3.x

- A modern Linux distribution (Ubuntu >= 20.04 is recommended)


#### Setup

```
sudo apt install bossa-cli python3-pip airspyhf wget unzip -y
```

```
git clone https://github.com/kholia/airspy-utils.git

cd airspy-utils

pip3 install -r requirements.txt
```

Install the [udev rules](60-airspy-hfplus.rules) and add your user to `plugdev`
to access the receiver and its bootloader serial port without sudo:

```sh
sudo groupadd -f plugdev
sudo usermod -aG plugdev "$(id -un)"
sudo install -m 644 60-airspy-hfplus.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

Log out and log back in to activate the group membership, then unplug and
reconnect the receiver so the rules take effect. Only this setup requires sudo;
the Python utilities and `bossac` commands below can then run as your user,
including over SSH. The rules grant read/write access to `plugdev` members.
The bootloader uses the generic Atmel SAM-BA ID `03eb:6124`, so the bootloader
rules also apply to other devices using that ID.


#### Usage

See the current firmware version (example):

```
$ python3 hfplus_fw.py
R3.0.6-CD
```

ATTENTION: Please verify if the firmware is `BB` or `CD`. This is IMPORTANT.

Expected firmware version after upgrading (CD example):

```
$ python3 hfplus_fw.py  # after firmware upgrade
R5.0.1-CD
```

Backup calibration:

```
$ python3 hfplus_ppb.py -d > file

$ hexdump file
0000000 71b0 a5ca 0000 0000 0000 0000 0000 0000
0000010 0000 0000 0000 0000 0000 0000 0000 0000
*
0000100
```

Read calibration (`ppb`) value:

```
$ python3 hfplus_ppb.py -r
0
```

Write calibration (`ppb`) value:

```
python3 hfplus_ppb.py -w -p 0
```


Reboot device in bootloader (programming) mode:

```
python3 hfplus_reboot.py -b
```

In bootloader mode, running `lsusb` should show something like:

```
Bus 003 Device 016: ID 03eb:6124 Atmel Corp. at91sam SAMBA bootloader
```

Reboot device in normal mode:

```
python3 hfplus_reboot.py -n -p /dev/ttyACM0
```

Note: You may need to replace `/dev/ttyACM0` with the actual value for your
setup. It might be `/dev/ttyACM1` for example.

Grab the official [R5.0.1 firmware](https://airspy.com/downloads/airspy-hf-flash-2024-12-11_5.0.1.zip)
(released 2024-12-11):

```
wget https://airspy.com/downloads/airspy-hf-flash-2024-12-11_5.0.1.zip

unzip airspy-hf-flash-2024-12-11_5.0.1.zip

cp airspy-hf-flash/hfplus-firmware-cd.bin firmware.bin  # ATTENTION: for devices using 'CD' firmware
# cp airspy-hf-flash/hfplus-firmware-bb.bin firmware.bin  # ATTENTION: for devices using 'BB' firmware
```

Full process to update the firmware (and restore the calibration):

Close any SDR software first. Use the `BB` or `CD` image matching the firmware
suffix recorded before entering bootloader mode, and save your calibration
value with `python3 hfplus_ppb.py -r` before flashing. After entering bootloader
mode, wait for `/dev/ttyACM0` to appear (adjust the path in every command if
necessary).

```
python3 hfplus_reboot.py -b

udevadm settle

bossac -u -p /dev/ttyACM0

bossac -e -b -v -p /dev/ttyACM0 -w firmware.bin

python3 hfplus_reboot.py -n -p /dev/ttyACM0

python3 hfplus_ppb.py -w -p 0  # use your own calibration value here
```

Check firmware version using the official software:

```
$ airspyhf_info
AirSpy HF library version: 1.6.8

S/N: 0xXYZ
Part ID: 0x00000002
Firmware Version: R5.0.1-CD
```


#### Firmware internals

Curious how the R5.0.1-CD firmware actually works (USB vendor commands,
streaming format, calibration flash, the SAM-BA reboot trick)? See
[docs/airspy-hf-firmware-internals.md](docs/airspy-hf-firmware-internals.md)
for independent reverse-engineering documentation, `hfplus_mkelf.py` to
wrap the raw `.bin` for IDA/Ghidra/radare2, and the [analysis/](analysis/)
folder for the annotated IDA database, decompiler dumps and reusable
scripts.


#### Open firmware

[openfirmware/](openfirmware/) contains **OpenHF+**, a from-scratch
replacement firmware for the HF+ Discovery (clean-room code + the
linux-firmware blob model for the STA709 tuner tables). It builds with
any `arm-none-eabi` toolchain, speaks the documented vendor protocol,
keeps your calibration page intact, and includes the SAM-BA reboot for
safe recovery. See its README for the roadmap and first-flash test plan.


#### References

- https://gist.github.com/jj1bdx/ce9eb3bd7320eed76396669a25f27e29

- https://airspy.com/downloads/hfplus_changelog.txt

- https://airspy.com/downloads/SDRSharp_The_Guide_v3.0_ENG.pdf

- https://airspy.com/downloads/airspy-hf-flash-2024-12-11_5.0.1.zip

- https://github.com/airspy/airspyhf/issues/18 ("funny" thread)

- https://github.com/airspy/airspyhf/issues/4


#### My consulting services

:green_heart: Are you looking for **commercial** support with this or similar stuff? I am [available
over email / phone](mailto:dhiru.kholia@gmail.com?subject=[GitHub]%20JtR%20Commercial%20Support%20Request&body=Hi%20-%20We%20are%20interested%20in%20purchasing%20commercial%20support%20options%20for%20your%20project.) for a chat. Note: Project sponsors get access to direct support.

- Reverse-engineering, and re-implementation of custom and/or proprietary
  software routines.

  e.g. https://github.com/openwall/john/blob/bleeding-jumbo/src/adxcrypt_fmt_plug.c

  https://github.com/kholia/4690-OS - such is life ;)

- Solid documentation work

  I provide customers with solid documentation. Example: https://github.com/kholia/OSX-KVM


#### Testimonials

- https://www.whitehatsec.com/blog/cracking-aes-256-dmgs-and-epic-self-pwnage/

- https://github.com/openwall/john/graphs/contributors
