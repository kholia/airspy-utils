# Airspy HF+ Discovery — Firmware Internals (R5.0.1-CD)

Independent reverse-engineering documentation of the closed-source receiver
firmware shipped in `airspy-hf-flash-2024-12-11_5.0.1.zip`
(`hfplus-firmware-cd.bin`, 28,616 bytes, SHA of zip contents dated
2024-12-11). Everything below was derived from the official firmware image,
the BSD-licensed `libairspyhf` host library, the public SAM3U datasheet, and
read-only probing of a live HF+ Discovery unit. No vendor source code was
used or is reproduced here.

Why does this document exist? Because the vendor refuses to document or
discuss firmware internals (see airspy/airspyhf#4 and #18), while the device
is marketed to and supported by the FOSS SDR community. Interoperability
documentation for hardware you own should not require an act of Congress.

Everything marked **[live]** was verified against a real device running
R5.0.1-CD. Everything else is static analysis with addresses you can check
yourself.

---

## 1. Hardware platform

### 1.1 Microcontroller — Atmel/Microchip ATSAM3U1 (Cortex-M3)

Evidence:

| Fact | Source |
|---|---|
| `bossac -i` reports `Device: ATSAM3U1`, flash applet base `0x80000`, 256 pages × 256 B | captured in `hfplus_reboot.py` comments |
| Flash mapped at `0x00080000` ("Internal Flash 0"), SRAM at `0x20000000`, dedicated UDPHS DMA RAM at `0x20180000` | SAM3U Series datasheet §8.1 (in `datasheets/`) |
| Vector table: initial SP `0x20001878`, reset `0x00083309` (Thumb) | first 8 bytes of the image |
| Bootloader enumerates as `03eb:6124` Atmel SAM-BA, flashed with `bossac` | this repo's README |

There is **no FPU** — a Cortex-M3. All float samples you see in SDR
applications are manufactured host-side; the wire format is pure int16
(§5.3), which is precisely what airspy/airspyhf#4 was about.

### 1.2 Tuner — ST STA709 (silicon rev "CD")

The tuner is programmed over **I²C via TWI0** (`0x40084000`). The firmware
contains a TWI0 driver family at `0x83B64–0x840D8` plus
`tuner_init` (`0x84F1C`), `tuner_set_frequency` (`0x843D0`), and
`tuner_write_table` (`0x83E7C`). The public changelog names the part:
"Added support for STA709CD silicon variant with Fail-Safe boot" (R2.1.0)
and "Updated the ST firmware" (R4.0.0). The two flash images
(`-bb`/`-cd`) exist because the two silicon revisions need different
initialization (§8).

The STA709 integrates the mixer/AGC chain **and the ADC**. Digitized IQ
leaves the tuner as a synchronous serial stream into the SAM3U's **SSC**
(`0x40004000`) operating as a link consumer, and **DMAC** (`0x400B0000`)
ferries buffers to the **UDPHS** USB 2.0 High-Speed device port
(`0x400A4000`). Hence the entire peripheral budget of the firmware is five
interrupts:

| NVIC IRQ | Peripheral | Vector | Handler role |
|---|---|---|---|
| 10 | PIOA | `0x812FC` | bit-banged control lines |
| 11 | PIOB | `0x813D4` | bit-banged control lines |
| 21 | SSC   | `0x839E0` | **stub** (`bx lr`) — errors ignored, data path is pure DMA |
| 28 | DMAC  | `0x8367C` | buffer completion engine |
| 29 | UDPHS | `0x81E0C` | USB device interrupt (control + streaming) |

Signal path:

```
Antenna → preselector/attenuator (PIO GPIO) → STA709 (TWI0 control,
integrated ADC) → SSC (serial IQ in) → DMAC → UDPHS FIFO (0x20180000)
→ USB bulk IN 0x81 → host (int16 IQ) → host-side DSP (libairspyhf)
```

## 2. Firmware image anatomy

Raw binary loaded at flash `0x00080000` (page 0). 28,616 bytes ≈ 112 pages
of the 256-page (64 KB) region the SAM-BA applet exposes.

| Region | Address | Content |
|---|---|---|
| Vector table | `0x80000`–`0x800B7` | 46 entries (16 system + 30 external); default handler `0x83304` |
| Code | `0x800BC`–`0x857BB` | ~21 KB Thumb-2 |
| Version string | `0x857BC` | `"R5.0.1-CD\0"` **[live]** |
| Filter/band tables | `0x85848`–`0x85AD4+` | per-sample-rate tuner register blobs (§8) |
| WinUSB descriptor | `0x86C48` | 40-byte MS OS compatible-ID descriptor (`"WINUSB"`), served by bRequest 48 |
| USB strings | `0x86F58`+ | `"AIRSPY HF+"`, `"www.airspy.com"`, `"AIRSPYHF SN:"` template + 16-hex-digit serial filled at runtime |
| Trailing `0xFF` run | end of image | flash padding |

Runtime memory (from `Reset_Handler` at `0x83308`):

- `.data`: 780 bytes copied from flash `0x86CBC` to `0x20000000`
- `.bss`: 868 bytes zeroed at `0x20001390`
- initial MSP `0x20001878` (stack top), VTOR set to `0x80000`
- the entire USB request state machine lives in `.data` around
  `0x20000400`–`0x20000580` (§3.2)

## 3. USB device anatomy

### 3.1 Interfaces and endpoints

- VID/PID `03eb:800c` in normal mode; `03eb:6124` (ROM SAM-BA) in bootloader
  mode.
- Configuration 1, interface 0 **alternate setting 1** exposes the streaming
  endpoint: **bulk IN `0x81`** (512-byte HS packets; the host library queues
  16 concurrent 16 KB transfers).
- Control transfers implement the vendor command set: `bmRequestType`
  `0x40` (OUT) / `0xC0` (IN), `bRequest` = command, `wValue`/`wIndex` =
  arguments, `wLength` = data phase size. `bRequest == 0` is accepted as a
  no-op ACK.

### 3.2 Control-transfer engine (from `udphs_irq` at `0x81E0C`)

SAM3U DMA deposits EP0 SETUP packets into the dedicated UDPHS RAM at
`0x20180000`; the ISR copies 8 bytes to a RAM structure at `0x20000408`
(`[+0]` bmRequestType, `[+1]` bRequest, `[+2..3]` wValue, `[+4..5]` wIndex,
`[+6..7]` wLength) and invokes the request router at `0x8090C`, which
dispatches standard requests and vendor requests (below). Control-OUT data
(`SET_FREQ`, `CONFIG_WRITE`) is likewise copied from `0x20180000` into the
staging buffer at `0x20000570` (256 bytes).

The vendor dispatcher (function `vendor_request_dispatch`, `0x8527C`) uses:

| RAM | Meaning |
|---|---|
| `0x20000410` | IN data pointer |
| `0x20000414` | IN response length |
| `0x20000418` | control-OUT completion callback |
| `0x2000041C` | IN data-phase fill callback |
| `0x20000570` | 256-byte staging buffer |

## 4. Vendor command reference

The dispatcher switch covers `bRequest` 0–0x69 (jump table at `0x852A8`,
106 entries). **Implemented: 0–15, 48, 100–105. Everything else —
including commands 16–22 that `airspyhf_commands.h` still publishes —
lands on a shared stall stub at `0x85450`.**

### 4.1 Implemented commands matching `airspyhf_commands.h`

| # | Name | Dir | Arguments / payload | Behavior in R5.0.1-CD |
|---|---|---|---|---|
| 1 | RECEIVER_MODE | OUT | wValue 0/1 | mailbox `[0x20000338]`, dirty flag `[0x20000339]`; 0 = standby ("low current standby mode" per changelog R1.6.1) |
| 2 | SET_FREQ | OUT | 4-byte **big-endian kHz** in control-OUT data | `set_frequency_khz(0x80148)` → band selection + tuner programming (§8) |
| 3 | GET_SAMPLERATES | IN | wIndex 0 → count; wIndex 7 → array | count = **7**; rates = 912000, 768000, 650000, 456000, 384000, 228000, 192000 Hz **[live]** |
| 4 | SET_SAMPLERATE | OUT | wIndex = rate index 0–6 | maps index → same table; mailbox `[0x200001A0]`, flag `[0x20000337]` |
| 5 | CONFIG_READ | IN | **wLength must be exactly 256** (other lengths stall) | `config_read_flash(buf, 256)` copies flash page `0x8FFC0`; returns raw contents (`0xFF…` when erased) — the magic check is host-side **[live]** |
| 6 | CONFIG_WRITE | OUT | 256-byte control-OUT data | refused while streaming (`is_streaming` `0x80234`); callback `0x85204` erases + rewrites page `0x8FFC0` via EEFC (§6) |
| 7 | GET_SERIALNO_BOARDID | IN | 20 bytes | `{ u32 part_id = 2 (Discovery); u64 serial; u64 serial2 }` **[live]** — host lib only consumes the first u64 |
| 8 | SET_USER_OUTPUT | OUT | wValue = pin, wIndex = state | `0x80240` (PIO user GPIO) |
| 9 | GET_VERSION_STRING | IN | up to 256 bytes | copies `"R5.0.1-CD"` **[live]** |
| 10 | SET_AGC | OUT | wValue 0/1 | `0x801E4` |
| 11 | SET_AGC_THRESHOLD | OUT | wValue | `0x801F8` |
| 12 | SET_ATT | OUT | wValue = attenuation step index | `0x8020C` |
| 13 | SET_LNA | OUT | wValue 0/1 | `0x80220` |
| 14 | GET_SAMPLERATE_ARCHITECTURES | IN | wIndex 0 → count; wIndex 7 → array | count = 7; array = `00 00 01 01 01 01 01`: **912 ksps and 768 ksps are Zero-IF; 650k and below are Low-IF** **[live]** |
| 15 | GET_FILTER_GAIN | IN | 1 byte | dB value; host applies `10^(−gain/20)` as conversion gain **[live]** (returned 0 at 7.2 MHz / default rate) |

### 4.2 Published-but-dead commands (STALL on R5.0.1-CD)

`GET_FREQ_DELTA` (16), `SET_VCTCXO_CALIBRATION` (17),
`SET_FRONTEND_OPTIONS` (18), `GET_ATT_STEPS` (19), `GET_BIAS_TEE_COUNT`
(20), `GET_BIAS_TEE_NAME` (21), `SET_BIAS_TEE` (22) — verified **[live]**
for 16/19/20/21, static for the rest. The host library is written to
degrade gracefully: it falls back to the built-in 9×6 dB attenuation table
for a Discovery, ignores frequency-delta/shortfalls, and treats bias-tee
absence as "feature not present". The real `SET_FRONTEND_OPTIONS` moved to
command 103 (§4.3). VCTCXO trimming simply does not exist on this hardware
generation; host-side ppb correction (§5.4) is the only calibration path.

### 4.3 Undocumented commands

| # | Purpose | Details |
|---|---|---|
| 48 (0x30) | Microsoft OS descriptor | requires `wIndex == 4`; returns up to 40 bytes from flash `0x86C48` — the WinUSB compatible-ID descriptor (driver-less Windows operation) |
| 100 (0x64) | diagnostics readout | fills staging buffer with internal measurement state (reads the PWM counter block `0x4008C000` and TWI0 state); returns `wLength ≤ 256` bytes **[live]** |
| 101 (0x65) | flash window read | streams up to 255 bytes from a fixed flash area via IN callback `0x851F0` **[live]** |
| **102 (0x66)** | **reboot to SAM-BA bootloader** | magic-only: `wValue == 0xC0DE && wIndex == 0xBABE`; see §7 |
| 103 (0x67) | SET_FRONTEND_OPTIONS (new number) | 32-bit flags = `wValue \| (wIndex << 16)` → `0x84A3C`; replies with 3 bytes **[live]** |
| 104 (0x68) | status query | requires `wLength == 6`; IN callback `0x851BC` |
| 105 (0x69) | string query | refused while streaming; IN callback `0x85198`, length = `wLength` |

## 5. Streaming

### 5.1 Start/stop

Host sequence (`libairspyhf`): `RECEIVER_MODE(0)` → `CLEAR_FEATURE(halt)`
on EP `0x81` → `RECEIVER_MODE(1)` → submit bulk transfers. Stop:
`RECEIVER_MODE(0)`. The firmware side implements receiver-on/off as a
mailbox consumed by the main loop, which starts/stops the SSC→DMAC→UDPHS
chain (`0x83920`/`0x839A0` configure SSC+DMAC; `0x83624` sets up DMA in
`main()`; endpoint DMA ping-pongs 64 KB buffer pairs per endpoint with
state at `0x200003D0`, 24 bytes per endpoint).

### 5.2 Wire format

Bulk IN `0x81`, 16 KB per transfer = **4096 interleaved complex int16 LE**
samples, memory layout `{ int16 im; int16 re; }` — imaginary first
(mirrored by `airspyhf_complex_int16_t` in the host library). Verified by
capturing 12 transfers (196,608 bytes = 49,152 complex samples) from the
live device at 7.2 MHz, all four transfers well-formed with noise-floor
statistics (rms ≈ 197 counts on an undriven input). **[live]**

At 912 ksps this is 7.3 MB/s of raw IQ — USB 2.0 High-Speed territory,
which is why the SAM3U (with its dedicated UDPHS DMA RAM) was chosen.

### 5.3 Host-side DSP contract (why the device can stay dumb)

The firmware sends *uncorrected* int16 IQ. `libairspyhf` then:

1. converts to float with gain `2⁻¹⁵ · 10^(−filter_gain_dB/20)` (command 15);
2. runs the **I/Q amplitude/phase balance correction** in software
   (`iqbalancer.c`) — needed because Zero-IF mode has analog I/Q
   imbalance;
3. applies **fine tuning**: in Zero-IF mode the host deliberately offsets
   the LO by +5 kHz (`DEFAULT_IF_SHIFT`) and rotates the spectrum back
   with a CORDIC-style recursive oscillator (`rotate_complex`), gaining
   1 Hz-class tuning resolution without touching the tuner;
4. applies **ppb calibration** purely host-side:
   `adjusted = freq · (1 + ppb·10⁻⁹)`.

This is the answer to issue #4: the device never produces floats, and the
"floats are the right representation" argument was about host API only.

### 5.4 Sample-rate architectures

R5.0.1-CD: indices 0–1 (912 ksps, 768 ksps) = Zero-IF; indices 2–6
(650k/456k/384k/228k/192k) = Low-IF **[live]**. This matches the
changelog trail: R2.6.0 introduced "768 ksps Zero-IF, 384/256/192 Low-IF"
and R4.0.0 replaced 256k with 228k. Low-IF moves the DC spike out of band
and lets the host drop the IQ balancer (`is_low_if` in the library).

## 6. Calibration / configuration flash

- Location: **flash page at `0x8FFC0`** — the last 256-byte page of the
  64 KB region, i.e. page 255, deliberately kept outside the firmware
  image.
- Layout (256 bytes, from `flash_config_t` + `hfplus_ppb.py` dumps):

```
0x00  u32  magic = 0xA5CA71B0   (bytes B0 71 CA A5)
0x04  i32  calibration_ppb      (host-side frequency correction)
0x08  i32  calibration_vctcxo   (legacy; unused by CD firmware)
0x0C  u32  frontend_options
0x10  ...  zero padding to 256 bytes
```

- `CONFIG_WRITE` (6) is rejected while streaming, then the completion
  callback `config_write_out_cb` (`0x85204`) runs the EEFC sequence:
  wait controller ready → erase page region `0x8FFC0–0x900BF` → write
  256 bytes → verify.
- This is why a `bossac` reflash destroys calibration (`-e` erases the
  whole region): back up with `python3 hfplus_ppb.py -d > file` and
  restore afterwards, exactly as the README instructs.

## 7. Reboot into the SAM-BA bootloader

Vendor request `bRequest 0x66` with `wValue 0xC0DE`, `wIndex 0xBABE`
(`hfplus_reboot.py` encodes this as `ctrl_transfer(0x40, 0x66, 0xc0de,
0xbabe, 0)`). Handler chain, fully decompiled:

```
reboot_to_samba (0x835F8):
    if (wValue == 0xC0DE && wIndex == 0xBABE):
        __disable_irq()
        prepare_boot_from_rom(1)          # 0x83280: GGPB (0x0D) then CGPB (0x0C) bit 1
        WDT_MR = 0xA5000005               # 0x400E1200: enable watchdog, ~ms timeout
        while (1);                        # spin -> watchdog reset
```

On SAM3U the ROM SAM-BA monitor is booted when **GPNVM bit 1 is 0**
(datasheet §8.2.3); the app runs from flash when it is 1. So the reboot
path reads the GPNVM bits (GGPB, EEFC cmd `0x0D`) and **clears** bit 1
(CGPB, cmd `0x0C`) before forcing the watchdog reset. Any *other*
wValue/wIndex is ignored and the request merely ACKs — which is why the
reboot scripts can probe safely. After flashing, `bossac --boot=1` sets
the bit again so the next reset boots the application from flash. There
is no USB acknowledgment of the reboot itself — the device simply dies
mid-request.

## 8. Tuner control (TWI0)

`tuner_set_frequency` (`0x843D0`) receives kHz and:

- ≤ 0x45 (69 kHz): out of range handling;
- ≤ 0x7918 (31.0 MHz): **HF band 5** — switches antenna-path GPIOs
  (`0x810A0(35)`, `0x810A0(36)` = PIO lines 35/36, preselector/LNA paths)
  and calls the band-5 table;
- 0xFA00–0x1CCF0 kHz (64–118.8 MHz): band 1 (VHF with LNA, line 36/35
  toggled the other way);
- higher ranges: further bands (band table continues through `0x843D0`).

After band selection it writes one of the per-sample-rate **filter tables**
(`dword_85848`, `dword_85A34`, `dword_85AD4` …) to three TWI0 sub-addresses
(1, 2, 8) via `tuner_write_table` (`0x83E7C`) — the STA709's IF filter
corners per bandwidth, matching the changelog notes about "optimized HF
filter corners" and "more configurable filters in the HF tuner" (R1.8.0).

`main()` (`0x83690`) boot order: clock/PMC init (`0x805BC`, `0x83470`,
`0x836EC`, `0x8356C`) → enable PIOA/PIOB IRQs → antenna GPIO defaults →
**`tuner_init` (`0x84F1C`)** (the 594-byte STA709 bring-up: register push,
this is where BB vs CD silicon diverge) → DMAC setup (`0x83624`) →
state-machine loop (`0x80830`/`0x80270`) that services the command
mailboxes (`0x20000337` rate, `0x20000339` mode, …).

AGC/ATT/LNA controls (`0x801E4`, `0x801F8`, `0x8020C`, `0x80220`) write
small state variables consumed by the same loop, which then issues TWI0
transactions to the STA709's AGC and attenuator registers and toggles the
LNA PIO line.

## 9. BB vs CD

- Identical vector table and load base (same ATSAM3U1, same linker
  script); first code difference at offset `0xCC`, immediately after the
  vector table.
- BB image is 42,056 bytes vs CD 28,616 — the older STA709 revision needs
  a substantially larger initialization blob. Both report `R5.0.1`.
- The `BB`/`CD` suffix in the version string selects which silicon's
  init path the firmware runs at boot; **flashing the wrong one leaves the
  tuner dead** (the device still enumerates — the MCU boots fine). This is
  the "ATTENTION: verify BB or CD" warning in the README.

## 10. Odds and ends

- **Suspend/resume**: the UDPHS ISR tracks bus suspend (`0x2000033E`
  counter) and re-attaches via D+/D− pull-up control (`0x85278` /
  `0x85298`) — the "Sleep/Resume mode" from changelog R2.9.8.
- **Streaming stop on disconnect**: EP0 reset/configuration events tear
  down streaming state (changelog R4.0.4 "Stop the streaming when the USB
  connection is closed") — visible in `udphs_irq`'s reset branch.
- The **SSC interrupt handler is an empty stub**; sample intake is handled
  entirely by DMA chained to USB, so sample loss shows up as USB underrun
  handling (changelog R2.2.0), not SSC IRQs.
- The WinUSB descriptor (command 48) is what makes the device work on
  Windows without a driver INF.

## 11. Reproducing this analysis

1. Download the official flash zip, extract `hfplus-firmware-cd.bin`.
2. Wrap the raw blob in a minimal ELF32/ARM header so analysis tools get
   the load address right (this repo: `hfplus_mkelf.py`):

   ```
   python3 hfplus_mkelf.py hfplus-firmware-cd.bin firmware.elf 0x80000 0x83309
   ```

3. Open the ELF in IDA/Ghidra/radare2 as ARM Little-Endian (Cortex-M3,
   Thumb-2). The vector table at `0x80000` seeds the five IRQ handlers;
   `Reset_Handler` is `0x83308`; the vendor dispatcher is `0x8527C` with
   its jump table at `0x852A8`.
4. Probe a live device (read-only): the short pyusb snippets in this
   document reproduce every **[live]** marker; the safe reboot test is
   `ctrl_transfer(0x40, 0x66, 0x1111, 0x2222, 0)` — wrong magic, ACK, no
   side effects (verified against the decompiled handler first).

An IDA database with all functions renamed as cited in this document can
be regenerated in minutes from the ELF; the renames used here are listed
throughout the text (e.g. `vendor_request_dispatch`, `reboot_to_samba`,
`tuner_init`, `udphs_irq`).

## 12. Sources

- `airspy-hf-flash-2024-12-11_5.0.1.zip` — official firmware package,
  including `hfplus_changelog.txt` (quoted version history)
- `libairspyhf` 1.6.8 sources — `airspyhf.c`, `airspyhf_commands.h`,
  `iqbalancer.c` (BSD-2)
- SAM3U Series datasheet (Atmel-6430G) — memory map §8.1, peripheral IDs
  §11.1, UDPHS/EEFC chapters
- A live Airspy HF+ Discovery, firmware R5.0.1-CD
- Issue context: https://github.com/airspy/airspyhf/issues/4
