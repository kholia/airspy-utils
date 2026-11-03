/*
 * main.c - OpenHF+ firmware entry point.
 *
 * Boot sequence mirrors the stock firmware's main() (protocol doc
 * section 8): clock tree, USB bring-up, then a mailbox-driven main
 * loop. The tuner (STA709 over TWI0) arrives in roadmap phase 4 - see
 * openfirmware/README.md.
 */

#include <stdint.h>
#include "sam3u.h"
#include "usb.h"
#include "protocol.h"

extern void stream_pump(void);

/* mailbox state (stream.c), consumed here */
extern volatile uint8_t receiver_mode;
extern volatile uint8_t receiver_mode_dirty;
extern volatile uint32_t current_samplerate;
extern volatile uint8_t samplerate_dirty;

/* Clock plan (identical constants to the stock firmware, verified in
 * the decompilation of its clock init):
 *   - 12 MHz crystal, 62 slow-clock startup
 *   - PLLA: 12 MHz / 2 x 16 = 96 MHz -> MCK (CPU + peripherals)
 *   - UTMI: 12 MHz x 40 = 480 MHz -> UDPHS
 */
static void clock_init(void)
{
    /* start the crystal and wait for it to stabilise */
    PMC_MOR = PMC_MOR_KEY | PMC_MOR_MOSCXTST(62) | PMC_MOR_MOSCXTEN;
    while ((PMC_SR & PMC_SR_MOSCXTS) == 0)
        ;

    /* select the crystal as MAINCK */
    PMC_MOR = PMC_MOR_KEY | PMC_MOR_MOSCSEL |
              PMC_MOR_MOSCXTST(62) | PMC_MOR_MOSCXTEN;
    while ((PMC_SR & (1u << 16)) == 0)   /* MOSCSELS */
        ;

    /* PLLA: ONE=1, MULA=15 (x16), PLLACOUNT=63, DIVA=2 */
    PMC_PLLAR = PLLA_ONE | PLLA_MULA(15) | PLLA_CNT(63) | PLLA_DIVA(2);
    while ((PMC_SR & PMC_SR_LOCKA) == 0)
        ;

    /* switch MCK: -> MAINCK -> PLLA, no prescaler */
    PMC_MCKR = (PMC_MCKR & ~PMC_MCKR_CSS(3)) | PMC_MCKR_CSS(PMC_MCKR_CSS_MAIN);
    while ((PMC_SR & PMC_SR_MCKRDY) == 0)
        ;
    PMC_MCKR = (PMC_MCKR & ~PMC_MCKR_CSS(3)) | PMC_MCKR_CSS(PMC_MCKR_CSS_PLLA);
    while ((PMC_SR & PMC_SR_MCKRDY) == 0)
        ;
}

int main(void)
{
    clock_init();

    usb_set_serial_hex(((uint64_t)CHIPID_CIDR1 << 32) | CHIPID_CIDR0);
    usb_init();

    __asm volatile ("cpsie i");

    /* mailbox loop, like the stock firmware: USB ISRs record intent,
     * this loop applies it - keeps tuner programming out of IRQ
     * context and gives the streaming pump a home. */
    for (;;) {
        if (samplerate_dirty) {
            samplerate_dirty = 0;
            /* phase 4: reprogram SSC/tuner decimation for the new rate */
        }
        if (receiver_mode_dirty) {
            receiver_mode_dirty = 0;
            if (receiver_mode)
                stream_start();
            else
                stream_reset();
        }
        stream_pump();
    }
}
