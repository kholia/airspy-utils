/*
 * sam3u1.ld - linker script for OpenHF+ on the ATSAM3U1.
 *
 * Layout matches the stock firmware image so the standard flashing
 * flow (hfplus_reboot.py -b + bossac) works unchanged:
 *
 *   flash (rwx) : ORIGIN = 0x00080000, 64 KB minus the config page
 *   config      : ORIGIN = 0x0008FFC0, last 256-byte page - RESERVED
 *                 for the calibration block (magic 0xA5CA71B0 + ppb),
 *                 compatible with hfplus_ppb.py and libairspyhf.
 *
 * SRAM usage mirrors the stock .data/.bss region at 0x20000000.
 */

ENTRY(Reset_Handler)

MEMORY
{
    flash (rx)  : ORIGIN = 0x00080000, LENGTH = 64K - 256
    config (r)  : ORIGIN = 0x0008FFC0, LENGTH = 256
    sram  (rwx) : ORIGIN = 0x20000000, LENGTH = 6K
}

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > flash

    .text :
    {
        *(.text*)
        *(.rodata*)
        KEEP(*(.keep.rodata*))
        . = ALIGN(4);
    } > flash

    .ARM.exidx : ALIGN(4)
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > flash

    _sidata = LOADADDR(.data);

    .data : ALIGN(4)
    {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > sram AT > flash

    .bss (NOLOAD) : ALIGN(4)
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > sram

    _estack = ORIGIN(sram) + LENGTH(sram);

    /* the config page must stay untouched by the image */
    ASSERT(SIZEOF(.isr_vector) + SIZEOF(.text) + SIZEOF(.ARM.exidx)
           + SIZEOF(.data) <= LENGTH(flash), "image overflows into config page")
}
