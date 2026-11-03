/*
 * startup.c - vector table and reset path for the OpenHF+ firmware.
 *
 * The image is linked at Internal Flash 0 (0x00080000), matching the
 * stock firmware layout (SAM3U memory map, datasheet section 8.1).
 * 46 vector entries: 16 system + 30 external IRQs (SAM3U has exactly
 * IRQ0..IRQ29 - docs/airspy-hf-firmware-internals.md section 1.1).
 */

#include <stdint.h>
#include "sam3u.h"

extern uint32_t _sdata, _edata, _sidata;   /* .data: RAM range, flash init */
extern uint32_t _sbss, _ebss;              /* .bss: RAM range */
extern int main(void);

void Reset_Handler(void);
void Default_Handler(void);
void udphs_irq(void);      /* src/usb.c */
void dmac_irq(void);       /* src/stream.c (phase 3) */
void ssc_irq(void);        /* src/stream.c (phase 3) */

/* Weak aliases so unimplemented handlers park in the default loop. */
void PIOA_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PIOB_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SSC_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void DMAC_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void UDPHS_Handler(void) __attribute__((weak, alias("Default_Handler")));

/* Initial stack top: end of the 6 KB SRAM block we use (linker script).
 * The stock firmware uses 0x20001878; ours lands nearby via _estack. */
extern uint32_t _estack;

__attribute__((section(".isr_vector"), used))
void (*const vectors[46])(void) = {
    (void (*)(void))(&_estack),  /* [0]  initial MSP            */
    Reset_Handler,               /* [1]  reset                  */
    Default_Handler,             /* [2]  NMI                    */
    Default_Handler,             /* [3]  HardFault              */
    Default_Handler,             /* [4]  MemManage              */
    Default_Handler,             /* [5]  BusFault               */
    Default_Handler,             /* [6]  UsageFault             */
    0, 0, 0, 0,                  /* [7..10] reserved            */
    Default_Handler,             /* [11] SVCall                 */
    Default_Handler,             /* [12] DebugMonitor           */
    0,                           /* [13] reserved               */
    Default_Handler,             /* [14] PendSV                 */
    Default_Handler,             /* [15] SysTick                */
    /* external IRQ 0..29 */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler,             /* IRQ 9                       */
    PIOA_Handler,                /* IRQ 10 PIOA                 */
    PIOB_Handler,                /* IRQ 11 PIOB                 */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler,             /* IRQ 20                      */
    SSC_Handler,                 /* IRQ 21 SSC                  */
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler, Default_Handler, Default_Handler,
    Default_Handler,             /* IRQ 27                      */
    DMAC_Handler,                /* IRQ 28 DMAC                 */
    UDPHS_Handler,               /* IRQ 29 UDPHS                */
};

void Reset_Handler(void)
{
    uint32_t *src, *dst;

    /* .data: copy from flash */
    src = &_sidata;
    for (dst = &_sdata; dst < &_edata; )
        *dst++ = *src++;

    /* .bss: zero */
    for (dst = &_sbss; dst < &_ebss; )
        *dst++ = 0;

    VTOR = (uint32_t)vectors;

    (void)main();
    for (;;) ;
}

void Default_Handler(void)
{
    for (;;) ;
}
