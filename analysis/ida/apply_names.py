"""
IDA Pro bootstrap for the Airspy HF+ Discovery firmware R5.0.1-CD.

Load analysis/ida/airspy-cd2.elf (ELF32/ARM wrapping hfplus-firmware-cd.bin
at flash 0x80000, entry 0x83309) into IDA, then run this script
(File -> Script file...). It can also be fed verbatim to the ida-mcp
"execute_python" tool of a running IDA instance.

What it does:
  1. seeds Thumb-2 code at the vector-table targets and known entry points
     (a raw firmware blob gives IDA's auto-analysis nothing to start from)
  2. waits for auto-analysis
  3. applies the function names and comments produced by the
     reverse-engineering session documented in
     docs/airspy-hf-firmware-internals.md

Run it again at any time; it is idempotent.
"""

import idaapi
import ida_auto
import idc

BASE = 0x80000
SIZE = 0x6FC8  # 28616 bytes, the whole R5.0.1-CD image

# Vector table targets (0x80000 + 4*n): reset, default handler and the
# five real IRQ handlers - PIOA(10), PIOB(11), SSC(21, stub), DMAC(28),
# UDPHS(29). See docs/airspy-hf-firmware-internals.md section 1.1.
SEEDS = [
    0x83308,  # Reset_Handler
    0x83304,  # Default_Handler (b .)
    0x812FC,  # PIOA IRQ
    0x813D4,  # PIOB IRQ
    0x839E0,  # SSC IRQ (empty stub)
    0x8367C,  # DMAC IRQ
    0x81E0C,  # UDPHS IRQ
    0x8527C,  # vendor request dispatcher
    0x8090C,  # EP0 request router
]

RENAMES = {
    # USB / control transfer engine
    0x8090C: "ep0_standard_request",
    0x8527C: "vendor_request_dispatch",
    0x81E0C: "udphs_irq",
    0x81C44: "udphs_irq_entry",
    0x81A60: "udphs_endpoint_dma",
    # vendor command implementations (bRequest -> function)
    0x801D0: "cmd_receiver_mode",
    0x85214: "cmd_set_freq_out_cb",
    0x80148: "set_frequency_khz",
    0x8015C: "cmd_set_samplerate",
    0x801E4: "cmd_set_agc",
    0x801F8: "cmd_set_agc_threshold",
    0x8020C: "cmd_set_att",
    0x80220: "cmd_set_lna",
    0x80240: "cmd_set_user_output",
    0x80260: "cmd_get_version",
    0x80170: "cmd_get_filter_gain",
    0x837FC: "cmd_get_serialno",
    0x84A3C: "cmd_set_frontend_options",   # bRequest 103 (0x67)
    0x83DD4: "cmd_read_diagnostics",       # bRequest 100 (0x64)
    0x80234: "is_streaming",
    # flash / config / bootloader
    0x835D8: "config_read_flash",
    0x85204: "config_write_out_cb",
    0x857A0: "flash_read",
    0x830F0: "flash_write_page",
    0x8322C: "flash_erase_region",
    0x835F8: "reboot_to_samba",            # bRequest 102 (0x66), magic C0DE/BABE
    0x83280: "prepare_boot_from_rom",      # EEFC GPNVM bit 1
    # tuner (STA709 over TWI0) and streaming
    0x84F1C: "tuner_init",
    0x843D0: "tuner_set_frequency",
    0x83E7C: "tuner_write_table",
    0x83898: "ssc_init",
    0x83920: "ssc_dmac_start",
    0x839A0: "ssc_dmac_stop",
    0x83624: "dmac_setup",
    # ISRs and startup
    0x812FC: "pioa_irq",
    0x813D4: "piob_irq",
    0x839E0: "ssc_irq_stub",
    0x8367C: "dmac_irq",
    0x83308: "Reset_Handler",
    0x83690: "main",
}

COMMENTS = {
    0x20000408: "SETUP packet: +0 bmRequestType, +1 bRequest, +2 wValue, +4 wIndex, +6 wLength",
    0x20000570: "256-byte EP0 staging buffer (control-OUT data, IN responses)",
    0x8FFC0: "calibration/config flash page (magic 0xA5CA71B0, ppb, vctcxo, frontend_options)",
    0x857BC: "version string 'R5.0.1-CD'",
    0x86C48: "40-byte WinUSB compatible-ID descriptor (served by bRequest 48)",
    0x852A8: "vendor command jump table, 106 entries (bRequest 0..0x69)",
    0x85450: "default case: stall stub for unimplemented bRequests (incl. 16-22)",
    0x8527C: "switch(bRequest): 0-15 real, 16-99 stall, 48 WinUSB, 100-105 undocumented",
    0x835F8: "wValue==0xC0DE && wIndex==0xBABE -> GPNVM boot-from-ROM + watchdog reset",
}


def main():
    for ea in SEEDS:
        idc.split_sreg_range(ea, "T", 1)
        idc.create_insn(ea)
    ida_auto.plan_and_wait(BASE, BASE + SIZE, True)
    for ea in SEEDS + list(RENAMES):
        if not idaapi.get_func(ea):
            idaapi.add_func(ea)
    ida_auto.plan_and_wait(BASE, BASE + SIZE, True)

    ok = 0
    for ea, name in sorted(RENAMES.items()):
        if idaapi.set_name(ea, name, idaapi.SN_NOWARN) or idc.set_name(ea, name):
            ok += 1
    for ea, text in COMMENTS.items():
        if ea >= BASE:  # flash-resident targets only; RAM comments need manual mapping
            idc.set_cmt(ea, text, 0)

    ida_auto.plan_and_wait(BASE, BASE + SIZE, True)
    print("renamed %d/%d functions, %d comments set" % (ok, len(RENAMES), sum(1 for c in COMMENTS if c >= BASE)))


main()
