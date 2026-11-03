/*
 * usb.c - minimal UDPHS USB 2.0 High-Speed device stack for OpenHF+.
 *
 * Scope: EP0 control transfers (standard + the Airspy vendor set) and
 * one bulk-IN streaming endpoint (0x81). Written from the SAM3U
 * datasheet (ch. 38) with the register usage patterns cross-checked
 * against the stock firmware's ISR (see analysis/output/bigs.json,
 * function udphs_irq).
 *
 * BRING-UP STATUS: compiles, but has NOT been validated on hardware
 * yet. The clock sequence and command semantics are verified; the
 * endpoint state machine needs eyes-on first flash. Keep the SAM-BA
 * recovery path (command 102 / hfplus_reboot.py -b) in mind.
 *
 * USB descriptors intentionally mirror what the stock firmware offers
 * (including the WinUSB MS descriptor served by vendor command 48) so
 * existing host software - libairspyhf, SDR software, hfplus_*.py -
 * binds without changes. VID/PID note in openfirmware/README.md.
 */

#include <stdint.h>

#include "sam3u.h"
#include "protocol.h"
#include "usb.h"
#include "support.h"

static usb_ctrl_t ctrl;

/* ---- descriptors --------------------------------------------------- */

static const uint8_t dev_desc[] = {
    18, 0x01,                     /* len, DEVICE */
    0x10, 0x02,                   /* bcdUSB 2.00 */
    0x00, 0x00, 0x00,             /* class, subclass, protocol */
    64,                           /* bMaxPacketSize0 */
    0xEB, 0x03, 0x0C, 0x80,       /* VID 0x03EB, PID 0x800C */
    0x01, 0x00,                   /* bcdDevice */
    0x01, 0x02, 0x03,             /* string indexes */
    0x01                          /* num configurations */
};

/* interface 0 has TWO alternate settings; altsetting 1 carries the
 * bulk-IN streaming endpoint - this is what libairspyhf selects. */
static const uint8_t cfg_desc[] = {
    /* configuration */
    9, 0x02,
    41, 0x00,                     /* total length (9+9+7+9+7) */
    0x01, 0x01, 0x00,             /* interfaces, config value, string */
    0x80, 50,                     /* bus powered, 100 mA */
    /* interface alt 0 */
    9, 0x04,
    0x00, 0x00, 0x00,             /* iface 0, alt 0, no endpoints */
    0xFF, 0xFF, 0xFF, 0x00,       /* vendor class */
    /* interface alt 1 */
    9, 0x04,
    0x00, 0x01, 0x01,             /* iface 0, alt 1, 1 endpoint */
    0xFF, 0xFF, 0xFF, 0x00,
    /* bulk IN endpoint 0x81, 512 bytes (HS) */
    7, 0x05,
    0x81, 0x02, 0x00, 0x02, 0x00
};

static const uint8_t langid[]   = { 4, 0x03, 0x09, 0x04 };
static const char s1[]          = "Airspy";       /* manufacturer */
static const char s2[]          = "Airspy HF+";   /* product */
static char s3[]                = "AIRSPYHF SN:0000000000000000"; /* 12+16+1 = filled at runtime */

/* MS OS descriptor, byte-for-byte the one the stock firmware serves
 * from flash 0x86C48 (vendor command 48, wIndex 4). */

/* ---- tiny helpers --------------------------------------------------- */

static void ep_stall(uint8_t ep)
{
    UDPHS_EPTSETSTA(ep) = EPT_STA_FRCESTALL;
}

static void usb_attach(void)
{
    UDPHS_CTRL &= ~(1u << 8);     /* DETACH off: pull up D+ */
}

void usb_set_serial_hex(uint64_t serial)
{
    static const char hex[] = "0123456789ABCDEF";
    for (int i = 0; i < 16; i++)
        s3[12 + i] = hex[(serial >> ((15 - i) * 4)) & 0xF];
}

/* ---- EP0 standard requests ---------------------------------------- */

static uint16_t str_desc(const char *s, uint8_t *buf, uint16_t maxlen)
{
    uint16_t n = (uint16_t)strlen(s);
    if (n > (maxlen - 2) / 2)
        n = (maxlen - 2) / 2;
    buf[0] = (uint8_t)(2 + 2 * n);
    buf[1] = 0x03;
    for (uint16_t i = 0; i < n; i++) {
        buf[2 + 2 * i] = (uint8_t)s[i];
        buf[3 + 2 * i] = 0;
    }
    return buf[0];
}

static int standard_request(void)
{
    const uint8_t *src = 0;
    uint16_t len = 0;
    uint8_t tmp[4];

    switch (ctrl.bRequest) {
    case 0x00: /* GET_STATUS */
        tmp[0] = tmp[1] = 0;
        src = tmp; len = 2;
        break;
    case 0x05: /* SET_ADDRESS (handled by hardware, ACK via status) */
        return 1;
    case 0x06: /* GET_DESCRIPTOR */
        switch (ctrl.wValue >> 8) {
        case 0x01: src = dev_desc; len = sizeof(dev_desc); break;
        case 0x02: src = cfg_desc;  len = sizeof(cfg_desc);  break;
        case 0x03:
            switch (ctrl.wValue & 0xFF) {
            case 0: src = langid; len = sizeof(langid); break;
            case 1: len = str_desc(s1, ctrl.ctrl_buf, sizeof(ctrl.ctrl_buf)); src = ctrl.ctrl_buf; break;
            case 2: len = str_desc(s2, ctrl.ctrl_buf, sizeof(ctrl.ctrl_buf)); src = ctrl.ctrl_buf; break;
            case 3: len = str_desc(s3, ctrl.ctrl_buf, sizeof(ctrl.ctrl_buf)); src = ctrl.ctrl_buf; break;
            default: return 0;
            }
            break;
        default:
            return 0;
        }
        break;
    case 0x08: /* GET_CONFIGURATION */
        tmp[0] = 1; src = tmp; len = 1;
        break;
    case 0x09: /* SET_CONFIGURATION */
        return 1;
    case 0x0B: /* SET_INTERFACE: alt 0 or 1 both fine */
        stream_reset();
        return 1;
    default:
        return 0;
    }

    if (len > ctrl.wLength)
        len = ctrl.wLength;
    if (len > sizeof(ctrl.ctrl_buf))
        len = sizeof(ctrl.ctrl_buf);
    memcpy(ctrl.ctrl_buf, src, len);
    ctrl.ctrl_len = len;
    ctrl.ctrl_xferred = 0;
    return 1;
}

/* ---- EP0 engine ---------------------------------------------------- */

static void ep0_setup(void)
{
    volatile uint8_t *fifo = (volatile uint8_t *)UDPHS_EP_FIFO(0);
    uint8_t n = (uint8_t)((UDPHS_EPTSTA(0) & EPT_STA_BYTE_COUNT) >> 20);

    if (n != 8) {                  /* malformed SETUP -> stall */
        ep_stall(0);
        return;
    }
    for (uint8_t i = 0; i < 8; i++)
        ((uint8_t *)&ctrl.bmRequestType)[i] = fifo[i];

    ctrl.state = 0;
    ctrl.ctrl_xferred = 0;
    ctrl.ctrl_len = 0;

    int ok;
    if ((ctrl.bmRequestType & 0x60) == 0x40)
        ok = vendor_request_dispatch(&ctrl);      /* commands.c */
    else
        ok = standard_request();

    if (!ok) {
        ep_stall(0);
        return;
    }

    if (ctrl.bmRequestType & 0x80) {  /* IN: host wants data */
        ctrl.state = 1;
        /* data phase driven from ep0_in() below */
    } else if (ctrl.wLength) {        /* OUT with data phase */
        ctrl.state = 1;
        ctrl.ctrl_len = ctrl.wLength;
    } else {
        ctrl.state = 2;               /* no data: status stage now */
        UDPHS_EPTSETSTA(0) = EPT_STA_TX_PKTRDY;
    }
}

static void ep0_in(void)
{
    volatile uint8_t *fifo = (volatile uint8_t *)UDPHS_EP_FIFO(0);
    uint16_t chunk = ctrl.ctrl_len - ctrl.ctrl_xferred;

    if (chunk > 64)
        chunk = 64;
    for (uint16_t i = 0; i < chunk; i++)
        fifo[i] = ctrl.ctrl_buf[ctrl.ctrl_xferred + i];
    ctrl.ctrl_xferred += chunk;
    UDPHS_EPTSETSTA(0) = EPT_STA_TX_PKTRDY;

    if (ctrl.ctrl_xferred >= ctrl.ctrl_len)
        ctrl.state = 2;               /* status stage next */
}

static void ep0_out(void)
{
    volatile uint8_t *fifo = (volatile uint8_t *)UDPHS_EP_FIFO(0);
    uint16_t n = (uint16_t)((UDPHS_EPTSTA(0) & EPT_STA_BYTE_COUNT) >> 20);

    if (ctrl.state == 1) {            /* data phase of a control-OUT */
        uint16_t room = ctrl.ctrl_len - ctrl.ctrl_xferred;
        if (n > room)
            n = room;
        for (uint16_t i = 0; i < n; i++)
            ctrl.ctrl_buf[ctrl.ctrl_xferred + i] = fifo[i];
        ctrl.ctrl_xferred += n;

        if (ctrl.ctrl_xferred >= ctrl.wLength) {
            ctrl.state = 2;
            /* payload complete - apply it (SET_FREQ / CONFIG_WRITE),
             * the same hook pattern the stock firmware uses */
            if ((ctrl.bmRequestType & 0x60) == 0x40)
                vendor_out_complete(&ctrl);
            UDPHS_EPTSETSTA(0) = EPT_STA_TX_PKTRDY;  /* status stage */
        }
    } else {                          /* status stage of control-IN */
        ctrl.state = 0;
    }
}

/* ---- endpoint (re)initialisation (bus reset path) ------------------ */

static void usb_endpoints_init(void)
{
    /* EP0: control, 64 bytes, 1 bank - same value the stock ISR writes */
    UDPHS_EPTCFG(0) = EPTCFG_SIZE_64 | EPTCFG_TYPE_CTRL | EPTCFG_BANKS_1;
    while ((UDPHS_EPTCFG(0) & (1u << 31)) == 0)
        ;                             /* EPT_MAPD */
    UDPHS_EPTCTLENB(0) = EPT_CTL_EPT_ENABL | EPT_CTL_INT_DIS_DMA;
}

static void usb_stream_ep_init(void)
{
    /* EP1: bulk IN, 512 bytes, 2 banks (the 0x81 streaming pipe) */
    UDPHS_EPTCFG(1) = EPTCFG_SIZE_512 | EPTCFG_DIR_IN |
                      EPTCFG_TYPE_BULK | EPTCFG_BANKS_2;
    while ((UDPHS_EPTCFG(1) & (1u << 31)) == 0)
        ;
    UDPHS_EPTCTLENB(1) = EPT_CTL_EPT_ENABL | EPT_CTL_AUTO_VALID | EPT_CTL_INT_DIS_DMA;
}

/* ---- init + IRQ ----------------------------------------------------- */

void usb_init(void)
{
    /* clocks: peripheral + UTMI (480 MHz from the 12 MHz crystal) */
    PMC_PCER0 = (1u << ID_UDPHS);
    PMC_UCKR |= PMC_UCKR_UPLLEN | PMC_UCKR_UPLLCOUNT(0xF);
    while ((PMC_SR & PMC_SR_LOCKU) == 0)
        ;

    /* CTRL: keep only bit 7 set - the exact reset-path value the stock
     * firmware writes on ENDRESET (observed in udphs_irq). */
    UDPHS_CTRL = (UDPHS_CTRL & 0x80u) | 0x80u;
    usb_endpoints_init();
    usb_stream_ep_init();

    ctrl.state = 0;
    usb_attach();

    UDPHS_ICR = 0xFFFFFFFF;
    UDPHS_IER = UDPHS_I_ENDRESET | UDPHS_I_EP(0) | UDPHS_I_DET_SUSPD;
    irq_enable_nvic(ID_UDPHS);
}

void udphs_irq(void)
{
    uint32_t pending = UDPHS_ISR & UDPHS_IMR;

    if (pending & UDPHS_I_ENDRESET) {
        UDPHS_ICR = UDPHS_I_ENDRESET;
        usb_endpoints_init();
        usb_stream_ep_init();
        stream_reset();
        return;
    }
    if (pending & UDPHS_I_DET_SUSPD) {
        UDPHS_ICR = UDPHS_I_DET_SUSPD;
        stream_reset();               /* stock stops streaming on detach */
        return;
    }
    if (pending & UDPHS_I_EP(0)) {
        uint32_t sta = UDPHS_EPTSTA(0);
        if (sta & EPT_STA_RX_SETUP) {
            ep0_setup();
            UDPHS_EPTCLRSTA(0) = EPT_STA_RX_SETUP;
        } else if (sta & EPT_STA_RX_BK_RDY) {
            ep0_out();
            UDPHS_EPTCLRSTA(0) = EPT_STA_RX_BK_RDY;
        } else if (sta & EPT_STA_TX_COMPLT) {
            if (ctrl.state == 1)
                ep0_in();
            UDPHS_EPTCLRSTA(0) = EPT_STA_TX_COMPLT;
        }
        return;
    }
    if (pending & UDPHS_I_EP(1)) {
        UDPHS_EPTCLRSTA(1) = EPT_STA_TX_COMPLT;
        return;
    }
}
