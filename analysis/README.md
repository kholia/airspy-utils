# Analysis artifacts — Airspy HF+ Discovery firmware R5.0.1-CD

Everything here supports
[docs/airspy-hf-firmware-internals.md](../docs/airspy-hf-firmware-internals.md).
Goal: let anyone pick up the reverse engineering without redoing the
plumbing — and extend it (see "Open threads" at the bottom).

```
analysis/
├── ida/
│   ├── airspy-cd2.elf        official hfplus-firmware-cd.bin wrapped in a
│   │                         minimal ELF32/ARM header (load 0x80000, entry
│   │                         0x83309) — regenerate with ../hfplus_mkelf.py
│   ├── airspy-cd2.elf.i64    annotated IDA database (40 renamed functions,
│   │                         comments at the interesting addresses)
│   └── apply_names.py        IDAPython script that rebuilds the names and
│                             comments from scratch on a fresh load
│                             (idempotent; also feeds the ida-mcp
│                             execute_python tool verbatim)
├── scripts/
│   ├── ida_mcp_call.py       command-line client for a running IDA MCP
│   │                         server (http://127.0.0.1:8737/mcp), pure
│   │                         stdlib JSON-RPC
│   ├── decode_jumptable.py   decode the vendor-request jump table from a
│   │                         raw firmware image (works for BB/CD/future)
│   ├── probe_device.py       read-only probe suite against a live device;
│   │                         reproduces every [live] marker in the doc
│   └── stream_capture.py     capture the raw int16 IQ bulk stream
└── output/                   decompiler dumps from the analysis session
    ├── vector_table.txt      the 46 vector-table entries, annotated
    ├── handlers.json         pseudocode of the vendor request dispatcher
    │                         (contains every case handler)
    ├── impl.json             pseudocode of command implementations, flash
    │                         config and reboot routines
    ├── bigs.json             pseudocode of main(), tuner init, ISRs +
    │                         per-function peripheral usage summary
    └── periphmap.json        function -> peripheral register usage map
```

## Quick start

1. **Firmware**: download the official zip (see the main README), extract
   `hfplus-firmware-cd.bin`. It is intentionally *not* redistributed here.
2. **IDA** (any version): open `ida/airspy-cd2.elf`, run
   `ida/apply_names.py` (File → Script file…). You now have the annotated
   database. Ghidra/radare2 users: load the same ELF and use the address
   tables in the doc; `hfplus_mkelf.py` re-wraps any raw image.
3. **Headless IDA over MCP**: with an ida-mcp server running:

   ```
   python3 scripts/ida_mcp_call.py ida/apply_names.py
   python3 scripts/ida_mcp_call.py --list-tools
   ```

4. **Live device** (read-only, safe):

   ```
   pip install pyusb
   python3 scripts/probe_device.py
   python3 scripts/stream_capture.py -f 7200 -n 4
   ```

## Gotchas learned the hard way

- **ELF wrapping**: a raw Cortex-M blob gives IDA's auto-analysis nothing
  to start with, and the loader defaults the unified ARM module to
  AArch64. The wrapper must use `p_offset = 84` (after both headers) —
  `p_offset = 0` silently shifts every address by +0x54 and wrecks
  jump-table recovery. `hfplus_mkelf.py` does it right.
- **Thumb seeding**: `apply_names.py` marks the vector targets as Thumb
  (`split_sreg_range(ea, "T", 1)`) before creating code — without that,
  IDA decodes ARM instructions at those addresses.
- **Live probing safety**: vendor requests with wrong magic/arguments are
  proven no-ops (check the decompiled handler first, every time). The
  only request that changes device state irreversibly-ish is 102 with the
  real `0xC0DE/0xBABE` magic — it reboots into SAM-BA (recoverable with
  `hfplus_reboot.py -n`).
- `CONFIG_READ` requires `wLength == 256` exactly; anything else stalls.

## Open threads ( unfinished, pull requests welcome )

- **BB image**: `hfplus-firmware-bb.bin` (42 KB) — same MCU/layout, first
  difference at offset 0xCC, and its dispatcher/jump table live at
  *different* addresses (at the CD table address you get garbage — find
  the BB dispatcher first, then pass its table address to
  `decode_jumptable.py`) and map the STA709-BB init path.
- **Tuner register tables**: the blobs at `0x85848`–`0x85AD4`+ are STA709
  register sequences per sample rate / band. Correlate with the filter
  corner changes mentioned in the changelog (R2.9.1, R3.0.x).
- **Streaming DMA chain**: `dmac_setup` (0x83624), `ssc_init` (0x83898),
  `ssc_dmac_start/stop` (0x83920/0x839A0) — recover the SSC data-word
  format (chord: changelog mentions 18-bit DDC output scaled to 16) and
  the exact DMA descriptor chain into the UDPHS FIFO at 0x20180000.
- **Commands 100–105**: 100 dumps live measurement state (PWM counters
  via 0x4008C000 + TWI0 state); 101/104/105 have IN callbacks
  (0x851F0 / 0x851BC / 0x85198) whose sources haven't been decompiled.

## Redistribution note

`ida/airspy-cd2.elf(.i64)` are derived from Airspy's proprietary
firmware blob and exist here for interoperability research on hardware
you own; the firmware itself is not (re)distributed in this repository.
Think before publishing binaries beyond your own machines.
