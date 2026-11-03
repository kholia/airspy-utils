/*
 * protocol.h - the Airspy HF+ USB vendor protocol, as implemented by the
 * stock R5.0.1-CD firmware and documented in
 * docs/airspy-hf-firmware-internals.md (section 4).
 *
 * The interface facts below (command numbers, argument encoding, response
 * lengths) are functional interface specifications, not creative
 * expression - they are what keeps this firmware interoperable with
 * libairspyhf, SDR software and the hfplus_*.py tools in this repo.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define USB_VID             0x03EB
#define USB_PID_NORMAL      0x800C
#define USB_PID_BOOTLOADER  0x6124

/* Vendor bmRequestType values */
#define VR_OUT  0x40
#define VR_IN   0xC0

/* Implemented command numbers (stock R5.0.1-CD set) */
#define CMD_RECEIVER_MODE           1   /* wValue: 0=off 1=on */
#define CMD_SET_FREQ                2   /* control-OUT data: 4-byte BE kHz */
#define CMD_GET_SAMPLERATES         3   /* wIndex 0 -> count, 7 -> array */
#define CMD_SET_SAMPLERATE          4   /* wIndex: rate index */
#define CMD_CONFIG_READ             5   /* wLength MUST be 256 */
#define CMD_CONFIG_WRITE            6   /* control-OUT data: 256 bytes */
#define CMD_GET_SERIALNO_BOARDID    7   /* 20 bytes */
#define CMD_SET_USER_OUTPUT         8
#define CMD_GET_VERSION_STRING      9
#define CMD_SET_AGC                 10
#define CMD_SET_AGC_THRESHOLD       11
#define CMD_SET_ATT                 12  /* wValue: attenuation step index */
#define CMD_SET_LNA                 13
#define CMD_GET_SAMPLERATE_ARCH     14  /* wIndex 0 -> count, 7 -> array */
#define CMD_GET_FILTER_GAIN         15  /* 1 byte, host gain = 10^(-dB/20) */
#define CMD_GET_MS_OS_DESCRIPTOR    48  /* wIndex must be 4, <= 40 bytes */
#define CMD_READ_DIAGNOSTICS        100 /* <= 256 bytes of live state */
#define CMD_READ_FLASH_WINDOW       101 /* <= 255 bytes */
#define CMD_REBOOT_BOOTLOADER       102 /* magic: wValue 0xC0DE wIndex 0xBABE */
#define CMD_SET_FRONTEND_OPTIONS    103 /* flags = wValue | wIndex<<16 */
#define CMD_GET_STATUS6             104 /* wLength must be 6 */
#define CMD_GET_STRING              105 /* refused while streaming */

/* Sample-rate table (identical to stock R5.0.1-CD) */
#define SAMPLERATE_COUNT    7
extern const uint32_t samplerates[SAMPLERATE_COUNT];
/* 0 = Zero-IF, 1 = Low-IF (per-rate architecture, same as stock) */
extern const uint8_t samplerate_arch[SAMPLERATE_COUNT];

/* Calibration/config block in the reserved flash page (see protocol doc
 * section 6; compatible with hfplus_ppb.py and libairspyhf). */
#define CONFIG_MAGIC    0xA5CA71B0u
typedef struct {
    uint32_t magic_number;
    int32_t  calibration_ppb;
    int32_t  calibration_vctcxo;
    uint32_t frontend_options;
    uint8_t  padding[256 - 16];
} config_page_t;

/* Board/part id reported by GET_SERIALNO_BOARDID: 2 = HF+ Discovery */
#define BOARD_PART_ID_DISCOVERY 2

#define FW_VERSION_STRING "R0.0.1-OPEN-CD"

#endif /* PROTOCOL_H */
