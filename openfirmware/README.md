# OpenHF+ — open firmware for the Airspy HF+ Discovery

A from-scratch, community-owned replacement firmware for the Airspy HF+
Discovery (ATSAM3U1 + STA709), interoperable with the existing host
ecosystem: `libairspyhf`, SoapySDR/SDR software, and the `hfplus_*.py`
tools in this repository.

**Status: phase 2 skeleton.** It builds (864 bytes!), speaks the full
documented USB vendor protocol, preserves the calibration page, and
implements the SAM-BA reboot for safe recovery. The USB device stack has
**not yet been validated on hardware** — first flash is a bring-up
exercise (see the test plan). The tuner and real streaming are the next
phases.

## Why (and why not a decompilation)

GitHub hosts famous "decompilation and recompilation" projects. Those
reconstruct C source that byte-matches a binary produced by a known
toolchain — feasible with person-years of effort, and legally they are
still derivative works of proprietary code (some get DMCA'd). A
derivative can't honestly be shipped as "open firmware", and this
project has no interest in licensing fights with Airspy.

Instead, OpenHF+ follows the **linux-firmware model**:

- All code here is **original clean-room work**, written from:
  - the public SAM3U datasheet (register-level),
  - the interoperability documentation in
    [docs/airspy-hf-firmware-internals.md](../docs/airspy-hf-firmware-internals.md)
    (facts about interfaces — command numbers, wire formats, timings),
  - live probing of the hardware.
- The parts that cannot be re-derived without the secret STA709
  datasheet — the tuner register/init tables — are kept as **isolated
  data blobs** in `blobs/` (extracted from the official image with
  `tools/extract_blobs.py`), exactly like Linux ships WiFi/GPU firmware
  blobs. Code and blobs never mix; the blobs are data, not source.

Practical notes:
- **VID/PID**: the firmware enumerates with Atmel's `03eb:800c` so
  existing host software binds. Those IDs belong to the vendor; treat
  that as personal-use interoperability, same as every alternative
  firmware project does.
- **Licensing**: this subtree currently falls under the repository's
  overall license. If you want maximum community uptake (PRs welcome!),
  consider dedicating `openfirmware/` to MIT or BSD-2 — decide before
  the first external contribution. `blobs/` stays "data, no license
  claimed".

## Layout

```
openfirmware/
├── include/
│   ├── sam3u.h       register definitions (datasheet-derived, cited)
│   ├── protocol.h    the documented vendor protocol surface
│   ├── usb.h         control-transfer context shared by usb/commands
│   └── support.h     freestanding mem/str routines
├── src/
│   ├── startup.c     vector table + reset path (46 entries, like stock)
│   ├── main.c        clock init (verified stock constants) + mailbox loop
│   ├── usb.c         UDPHS driver: EP0 engine + bulk-IN endpoint
│   ├── commands.c    the vendor command set (protocol doc section 4)
│   ├── eefc.c        flash: calibration page, GPNVM, SAM-BA reboot
│   ├── stream.c      receiver mode + test-pattern streaming (phase 2)
│   └── support.c     memset/memcpy/strlen/strncpy
├── ld/sam3u1.ld      flash 0x80000, config page 0x8FFC0 RESERVED
├── tools/extract_blobs.py
└── blobs/            STA709 register tables (data only)
```

## Build

```
sudo apt install gcc-arm-none-eabi     # or any arm-none-eabi toolchain
cd openfirmware && make
```

Produces `build/openhf.bin` — flashable exactly like the stock image
(same load address, same layout rules, config page preserved).

## Flashing and recovery (read before first flash!)

The SAM-BA ROM bootloader makes the device practically unbrickable: you
can always force it with the ERASE button (see the official
`hfplus_erase.jpg`) and reflash over `/dev/ttyACM0`.

1. Back up calibration: `python3 hfplus_ppb.py -d > calib.bin`
2. Enter the bootloader: `python3 hfplus_reboot.py -b`
3. Flash: `bossac -e -b -v -p /dev/ttyACM0 -w openfirmware/build/openhf.bin`
4. Boot: `python3 hfplus_reboot.py -n -p /dev/ttyACM0`
5. Restore calibration: `python3 hfplus_ppb.py -w -p <value>`

The firmware also implements the standard in-USB reboot (vendor request
102 with `0xC0DE/0xBABE`), so once it runs you can re-enter SAM-BA
without touching the button.

## Bring-up test plan (first hardware contact)

Ordered so each step isolates one layer:

1. **Enumerates?** `lsusb` should show `03eb:800c` and `dmesg` a new
   high-speed device. If not: clock/UDPHS init — check `PMC_SR.LOCKU`,
   EPT_MAPD bits set on both endpoints.
2. **Control path?** `python3 analysis/scripts/probe_device.py` —
   version string, samplerates, architectures, serial (from CHIPID)
   should all answer. This exercises EP0 in both directions.
3. **Calibration survives?** `hfplus_ppb.py -d` before and after a
   config write round-trip.
4. **Reboot works?** `python3 hfplus_reboot.py -b` must land in SAM-BA
   (`03eb:6124`), and `-n` must come back.
5. **Streams?** `python3 analysis/scripts/stream_capture.py -n 4` —
   with the phase-2 test pattern you should see a clean ramp (min/max
   at ±32767 boundaries of the sweep), not noise.

Debug aids: the ELF has full symbols; open it alongside the annotated
stock database in `analysis/ida/` to compare register flows.

## Roadmap

- **Phase 1 — done**: protocol + hardware documentation, host tools.
- **Phase 2 — this skeleton**: USB device, full command set, flash
  config, SAM-BA reboot, test-pattern streaming. *Needs first-hardware
  validation.*
- **Phase 3 — real streaming**: SSC slave intake from the STA709
  (0x40004000) chained via DMAC into the EP1 DPRAM window; sample
  format per the protocol doc (interleaved int16 IQ, im-first). The
  stock architecture (5 interrupts, mailbox main loop) is already
  mirrored here.
- **Phase 4 — the tuner**: TWI0 driver, band logic from
  `tuner_set_frequency` (HF ≤ 31 MHz with antenna-path GPIOs 35/36,
  VHF bands above), filter tables from `blobs/`, AGC/ATT/LNA controls.
- **Phase 5 — niceties**: the undocumented diagnostics (100–105),
  suspend/resume power handling, maybe an open control port.

## Contributing

PRs welcome — especially: first-hardware bring-up logs, the SSC/DMAC
streaming chain, TWI0 traces against the stock firmware (logic analyzer
on the I²C pins is extremely helpful), and STA709 register semantics.
Keep new code original; anything learned from the stock binary belongs
in docs or comments as facts, not as reconstructed source.
