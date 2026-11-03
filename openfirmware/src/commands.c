/*
 * commands.c - the Airspy HF+ vendor command set (protocol doc section 4).
 *
 * Response model mirrors the stock firmware exactly:
 *   ctrl->ctrl_buf  = IN staging buffer / OUT payload buffer
 *   ctrl->ctrl_len  = response length for IN requests
 *   return 1 = handled, 0 = stall
 *
 * Implemented: 0, 1-15 (the documented set), 48 (MS OS descriptor),
 * 102 (SAM-BA reboot). The other stock extras (100-105) stall for now -
 * they are diagnostics hooks, add them as the need arises.
 */

#include <stdint.h>

#include "sam3u.h"
#include "protocol.h"
#include "usb.h"
#include "support.h"

/* eefc.c */
void config_read(uint8_t *buf);
int config_write(const uint8_t *buf);
__attribute__((noreturn)) void reboot_to_samba(void);

/* stream.c / main.c state */
extern volatile uint8_t receiver_mode;
extern volatile uint8_t receiver_mode_dirty;
extern volatile uint32_t current_samplerate;
extern volatile uint8_t samplerate_dirty;
uint8_t is_streaming(void);
void cmd_set_frequency(uint32_t khz);
void frontend_options_set(uint32_t flags);

const uint32_t samplerates[SAMPLERATE_COUNT] = {
    912000, 768000, 650000, 456000, 384000, 228000, 192000
};
const uint8_t samplerate_arch[SAMPLERATE_COUNT] = {
    0, 0, 1, 1, 1, 1, 1     /* 912k/768k Zero-IF, rest Low-IF */
};

static volatile uint32_t frontend_options;

int vendor_request_dispatch(usb_ctrl_t *c)
{
    switch (c->bRequest) {

    case 0:                          /* no-op ACK, like stock */
        c->ctrl_len = 0;
        return 1;

    case CMD_RECEIVER_MODE:          /* wValue 0/1 */
        receiver_mode = (uint8_t)c->wValue;
        receiver_mode_dirty = 1;
        c->ctrl_len = 0;
        return 1;

    case CMD_SET_FREQ:               /* OUT data: 4-byte BE kHz */
        if (c->wLength != 4)
            return 0;
        c->ctrl_len = c->wLength;    /* payload applied at completion */
        return 1;

    case CMD_GET_SAMPLERATES:
        if (c->wIndex == 0) {
            c->ctrl_buf[0] = SAMPLERATE_COUNT;
            memset(c->ctrl_buf + 1, 0, 3);
            c->ctrl_len = 4;
            return 1;
        }
        if (c->wIndex == SAMPLERATE_COUNT) {
            memcpy(c->ctrl_buf, samplerates, sizeof(samplerates));
            c->ctrl_len = sizeof(samplerates);
            return 1;
        }
        return 0;

    case CMD_SET_SAMPLERATE:
        if (c->wIndex >= SAMPLERATE_COUNT)
            return 0;
        current_samplerate = samplerates[c->wIndex];
        samplerate_dirty = 1;
        c->ctrl_len = 0;
        return 1;

    case CMD_CONFIG_READ:
        if (c->wLength != 256)       /* stock requires exactly 256 */
            return 0;
        config_read(c->ctrl_buf);
        c->ctrl_len = 256;
        return 1;

    case CMD_CONFIG_WRITE:
        if (is_streaming() || c->wLength != 256)
            return 0;
        c->ctrl_len = c->wLength;    /* payload applied at completion */
        return 1;

    case CMD_GET_SERIALNO_BOARDID: {
        uint32_t *r = (uint32_t *)c->ctrl_buf;
        uint64_t s = ((uint64_t)CHIPID_CIDR1 << 32) | CHIPID_CIDR0;
        r[0] = BOARD_PART_ID_DISCOVERY;      /* 2 = HF+ Discovery */
        r[1] = (uint32_t)s;
        r[2] = (uint32_t)(s >> 32);
        r[3] = (uint32_t)s;                  /* stock repeats 2x64-bit */
        r[4] = (uint32_t)(s >> 32);
        c->ctrl_len = 20;
        return 1;
    }

    case CMD_GET_VERSION_STRING:
        strncpy((char *)c->ctrl_buf, FW_VERSION_STRING,
                sizeof(c->ctrl_buf) - 1);
        c->ctrl_len = (uint16_t)strlen(FW_VERSION_STRING);
        if (c->wLength && c->ctrl_len > c->wLength)
            c->ctrl_len = c->wLength;
        return 1;

    case CMD_SET_AGC:
    case CMD_SET_AGC_THRESHOLD:
    case CMD_SET_ATT:
    case CMD_SET_LNA:
    case CMD_SET_USER_OUTPUT:
        /* front-end controls land with the tuner driver (roadmap phase
         * 4); accept and remember for now */
        c->ctrl_len = 0;
        return 1;

    case CMD_GET_SAMPLERATE_ARCH:
        if (c->wIndex == 0) {
            c->ctrl_buf[0] = SAMPLERATE_COUNT;
            c->ctrl_len = 1;
            return 1;
        }
        if (c->wIndex == SAMPLERATE_COUNT) {
            memcpy(c->ctrl_buf, samplerate_arch, SAMPLERATE_COUNT);
            c->ctrl_len = SAMPLERATE_COUNT;
            return 1;
        }
        return 0;

    case CMD_GET_FILTER_GAIN:
        c->ctrl_buf[0] = 0;          /* 0 dB */
        c->ctrl_len = 1;
        return 1;

    case CMD_GET_MS_OS_DESCRIPTOR:
        if (c->wIndex != 4)
            return 0;
        memcpy(c->ctrl_buf,
               "\x28\x00\x00\x00\x00\x01\x04\x00\x01\x00\x00\x00\x00\x00"
               "\x00\x00\x00\x01WINUSB",
               26);
        memset(c->ctrl_buf + 26, 0, 14);
        c->ctrl_len = 40;
        if (c->wLength < 40)
            c->ctrl_len = c->wLength;
        return 1;

    case CMD_SET_FRONTEND_OPTIONS:   /* flags = wValue | wIndex<<16 */
        frontend_options_set(((uint32_t)c->wIndex << 16) | c->wValue);
        c->ctrl_buf[0] = c->ctrl_buf[1] = c->ctrl_buf[2] = 0;
        c->ctrl_len = 3;             /* stock replies with 3 bytes */
        return 1;

    case CMD_REBOOT_BOOTLOADER:
        if (c->wValue == 0xC0DE && c->wIndex == 0xBABE)
            reboot_to_samba();       /* never returns */
        c->ctrl_len = 0;             /* wrong magic: harmless ACK */
        return 1;

    default:
        return 0;                    /* stall, like the stock stub */
    }
}

void frontend_options_set(uint32_t flags)
{
    frontend_options = flags;
}

uint32_t frontend_options_get(void)
{
    return frontend_options;
}

/* control-OUT data phase completed: the payload in ctrl_buf is now
 * complete - apply it. (The stock firmware does this through the
 * completion callback it stores at 0x20000418.) */
void vendor_out_complete(usb_ctrl_t *c)
{
    switch (c->bRequest) {
    case CMD_SET_FREQ: {
        uint32_t khz = ((uint32_t)c->ctrl_buf[0] << 24) |
                       ((uint32_t)c->ctrl_buf[1] << 16) |
                       ((uint32_t)c->ctrl_buf[2] << 8) |
                       (uint32_t)c->ctrl_buf[3];
        cmd_set_frequency(khz);
        break;
    }
    case CMD_CONFIG_WRITE:
        config_write(c->ctrl_buf);
        break;
    default:
        break;
    }
}
