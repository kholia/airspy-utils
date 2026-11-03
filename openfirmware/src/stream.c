/*
 * stream.c - receiver-mode handling and the streaming pipe.
 *
 * PHASE 2 SKELETON: with receiver mode ON the bulk-IN endpoint pushes a
 * deterministic test pattern (interleaved int16 IQ ramp) so the whole
 * USB path can be validated with the stock host tools:
 *
 *   python3 analysis/scripts/stream_capture.py -f 7200 -n 4
 *
 * PHASE 3 (see openfirmware/README.md roadmap): replace the pattern
 * generator with the real pipeline - SSC slave intake from the STA709
 * (0x40004000) chained via DMAC (0x400B0000) into the EP1 DPRAM
 * window, following the same buffer architecture the stock firmware
 * uses (docs/airspy-hf-firmware-internals.md section 5).
 */

#include <stdint.h>
#include "sam3u.h"
#include "usb.h"
#include "protocol.h"

volatile uint8_t receiver_mode;
volatile uint8_t receiver_mode_dirty;
volatile uint32_t current_samplerate = 912000;
volatile uint8_t samplerate_dirty;

static uint16_t ramp;                /* test-pattern state */

uint8_t is_streaming(void)
{
    return receiver_mode;
}

void stream_reset(void)
{
    ramp = 0;
}

void stream_start(void)
{
    /* nothing extra while the pattern generator is the source */
}

void cmd_set_frequency(uint32_t khz)
{
    (void)khz;                       /* tuner driver arrives in phase 4 */
}

/* Called from the main loop while receiver mode is on. Fills the EP1
 * FIFO window with one 512-byte packet (128 complex int16 samples) and
 * hands it to the host. AUTO_VALID on EP1 validates the bank for us. */
void stream_pump(void)
{
    if (!receiver_mode)
        return;

    if (UDPHS_EPTSTA(1) & EPT_STA_TX_PKTRDY)
        return;                      /* bank still busy */

    volatile int16_t *fifo = (volatile int16_t *)UDPHS_EP_FIFO(1);
    for (int i = 0; i < 256; i++) {
        fifo[i] = (int16_t)(ramp ^ 0x8000);   /* signed ramp */
        ramp += 257;                           /* ~5 kHz sweep at 912k */
    }
    UDPHS_EPTSETSTA(1) = EPT_STA_TX_PKTRDY;
}
