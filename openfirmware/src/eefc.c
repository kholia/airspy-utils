/*
 * eefc.c - flash / EEFC / GPNVM / forced-reset helpers.
 *
 * Implements the pieces the stock firmware exposes over USB:
 *  - read/write of the 256-byte calibration page at 0x8FFC0
 *    (commands CONFIG_READ/CONFIG_WRITE, compatible with hfplus_ppb.py)
 *  - the SAM-BA reboot path: clear GPNVM bit 1 (ROM boot), then force a
 *    watchdog reset. Verified semantics - see
 *    docs/airspy-hf-firmware-internals.md section 7.
 */

#include <stdint.h>
#include "sam3u.h"
#include "protocol.h"

#define CONFIG_PAGE ((volatile config_page_t *)CONFIG_PAGE_ADDR)

static void eefc_wait_ready(void)
{
    while ((EEFC0_FSR & EEFC_FSR_FRDY) == 0)
        ;
}

static void eefc_command(uint8_t cmd, uint8_t arg)
{
    eefc_wait_ready();
    EEFC0_FCR = EEFC_FCR_KEY | EEFC_FCR_FCMD(cmd) | EEFC_FCR_FARG(arg);
    eefc_wait_ready();
}

uint32_t eefc_get_gpnvm(void)
{
    eefc_command(EEFC_CMD_GGPB, 0);
    return EEFC0_FRR;
}

/* Clear GPNVM bit 1 so the next reset boots the ROM SAM-BA monitor
 * (GPNVM1 = 0 -> ROM, = 1 -> flash; datasheet 8.2.3). */
static void eefc_boot_from_rom(void)
{
    if (eefc_get_gpnvm() & (1u << GPNVM_BOOT_MODE))
        eefc_command(EEFC_CMD_CGPB, GPNVM_BOOT_MODE);
}

/* Read the calibration page (any contents, erased = 0xFF) into buf. */
void config_read(uint8_t *buf)
{
    volatile uint8_t *p = (volatile uint8_t *)CONFIG_PAGE_ADDR;
    for (unsigned i = 0; i < FLASH_PAGE_SIZE; i++)
        buf[i] = p[i];
}

/* Program the calibration page from buf. EWP (0x03) erases the single
 * page and writes it in one command. The page lives outside the firmware
 * image (reserved in the linker script), so this never touches running
 * code. */
int config_write(const uint8_t *buf)
{
    volatile uint32_t *p = (volatile uint32_t *)CONFIG_PAGE_ADDR;
    const uint32_t *w = (const uint32_t *)buf;
    uint8_t page = (CONFIG_PAGE_ADDR - FLASH_BASE) / FLASH_PAGE_SIZE;

    /* latch the 64 words of the page into the flash latch buffer */
    for (unsigned i = 0; i < FLASH_PAGE_SIZE / 4; i++)
        p[i] = w[i];

    eefc_command(EEFC_CMD_EWP, page);

    return 0;
}

/* The reboot-to-bootloader path, byte-for-byte equivalent in effect to
 * the stock firmware's reboot_to_samba (0x835F8). Never returns. */
__attribute__((noreturn))
void reboot_to_samba(void)
{
    __asm volatile ("cpsid i");
    eefc_boot_from_rom();
    WDT_MR = WDT_KEY | 5u;   /* ~ms timeout at 128 slow-clock periods */
    for (;;) ;
}
