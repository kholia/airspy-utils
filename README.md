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
sudo apt install bossa-cli python3-pip airspyhf -y
```

```
git clone https://github.com/kholia/airspy-utils.git

cd airspy-utils

pip3 install -r requirements.txt
```


#### Usage

See firmware version:

```
$ python3 hfplus_fw.py
R3.0.6-CD
```

ATTENTION: Please verify if the firmware is `BB` or `CD`. This is IMPORTANT.

See firmware version:

```
$ python3 hfplus_fw.py  # after firmware upgrade
R3.0.7-CD
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

Grab official firmware:

```
wget https://airspy.com/downloads/airspy-hf-flash-20200604.zip

unzip airspy-hf-flash-20200604.zip

cp airspy-hf-flash/hfplus-firmware-cd.bin firmware.bin  # ATTENTION: for devices using 'CD' firmware
# cp airspy-hf-flash/hfplus-firmware-bb.bin firmware.bin  # ATTENTION: for devices using 'BB' firmware
```

Full process to update the firmware (and restore the calibration):

```
python3 hfplus_reboot.py -b

sudo bossac -u -p /dev/ttyACM0  # you can try without sudo too

sudo bossac -e -b -v -p /dev/ttyACM0 -w firmware.bin

python3 hfplus_reboot.py -n -p /dev/ttyACM0

python3 hfplus_ppb.py -w -p 0  # use your own calibration value here
```

Check firmware version using the official software:

```
$ airspyhf_info
AirSpy HF library version: 1.6.8

S/N: 0xXYZ
Part ID: 0x00000002
Firmware Version: R3.0.7-CD
Available sample rates: 912 kS/s 768 kS/s 456 kS/s 384 kS/s 256 kS/s 192 kS/s
```


#### References

- https://gist.github.com/jj1bdx/ce9eb3bd7320eed76396669a25f27e29

- https://airspy.com/downloads/hfplus_changelog.txt

- https://airspy.com/downloads/SDRSharp_The_Guide_v3.0_ENG.pdf

- https://airspy.com/downloads/airspy-hf-flash-20200604.zip

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
