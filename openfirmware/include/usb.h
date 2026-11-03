#ifndef USB_H
#define USB_H

#include <stdint.h>

/* control-transfer context shared between usb.c and commands.c */
typedef struct {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;

    uint8_t  ctrl_buf[256];   /* OUT payload / IN staging buffer */
    uint16_t ctrl_len;        /* bytes of data phase */
    uint16_t ctrl_xferred;
    uint8_t  state;
    uint8_t  zlp_pending;
} usb_ctrl_t;

void usb_init(void);
void usb_set_serial_hex(uint64_t serial);

/* vendor hooks (commands.c): dispatch at SETUP time; out_complete when
 * a control-OUT data phase finishes (payload now fully in ctrl_buf) */
int vendor_request_dispatch(usb_ctrl_t *c);
void vendor_out_complete(usb_ctrl_t *c);

/* implemented in stream.c */
void stream_reset(void);
void stream_start(void);

#endif /* USB_H */
