/* CDC descriptor layout adapted from libopencm3-examples usb_cdcacm/cdcacm.c.
 * Copyright (C) 2010 Gareth McMullin <gareth@blacksphere.co.nz>
 * Modifications: CarHUB prototype, 2026.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 * See ../THIRD_PARTY.md and ../licenses/LGPL-3.0.txt.
 */
#include "usb_link.h"
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>

static usbd_device *device;
static uint8_t control[128], tx[512];
static unsigned head, tail, count;
static uint32_t dropped;
static bool configured;
static char serial[25];

static const struct usb_device_descriptor descriptor = {
    .bLength = USB_DT_DEVICE_SIZE, .bDescriptorType = USB_DT_DEVICE,
    .bcdUSB = 0x0200, .bDeviceClass = USB_CLASS_CDC, .bMaxPacketSize0 = 64,
    /* ST demonstration IDs: local prototype only, not a registered product ID. */
    .idVendor = 0x0483, .idProduct = 0x5740, .bcdDevice = 0x0100,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1,
};
static const struct usb_endpoint_descriptor notification[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE, .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x83, .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
    .wMaxPacketSize = 16, .bInterval = 255,
}};
static const struct usb_endpoint_descriptor data[] = {{
    .bLength = USB_DT_ENDPOINT_SIZE, .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x01, .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64, .bInterval = 1,
}, {
    .bLength = USB_DT_ENDPOINT_SIZE, .bDescriptorType = USB_DT_ENDPOINT,
    .bEndpointAddress = 0x82, .bmAttributes = USB_ENDPOINT_ATTR_BULK,
    .wMaxPacketSize = 64, .bInterval = 1,
}};
static const struct {
    struct usb_cdc_header_descriptor header;
    struct usb_cdc_call_management_descriptor call;
    struct usb_cdc_acm_descriptor acm;
    struct usb_cdc_union_descriptor association;
} __attribute__((packed)) functional = {
    .header = { .bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
        .bDescriptorType = CS_INTERFACE, .bDescriptorSubtype = USB_CDC_TYPE_HEADER, .bcdCDC = 0x0110 },
    .call = { .bFunctionLength = sizeof(struct usb_cdc_call_management_descriptor),
        .bDescriptorType = CS_INTERFACE, .bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
        .bmCapabilities = 0, .bDataInterface = 1 },
    .acm = { .bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
        .bDescriptorType = CS_INTERFACE, .bDescriptorSubtype = USB_CDC_TYPE_ACM, .bmCapabilities = 2 },
    .association = { .bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
        .bDescriptorType = CS_INTERFACE, .bDescriptorSubtype = USB_CDC_TYPE_UNION,
        .bControlInterface = 0, .bSubordinateInterface0 = 1 },
};
static const struct usb_interface_descriptor comm[] = {{
    .bLength = USB_DT_INTERFACE_SIZE, .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 0, .bNumEndpoints = 1, .bInterfaceClass = USB_CLASS_CDC,
    .bInterfaceSubClass = USB_CDC_SUBCLASS_ACM, .bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
    .endpoint = notification, .extra = &functional, .extralen = sizeof(functional),
}};
static const struct usb_interface_descriptor stream[] = {{
    .bLength = USB_DT_INTERFACE_SIZE, .bDescriptorType = USB_DT_INTERFACE,
    .bInterfaceNumber = 1, .bNumEndpoints = 2, .bInterfaceClass = USB_CLASS_DATA,
    .endpoint = data,
}};
static const struct usb_interface interfaces[] = {
    { .num_altsetting = 1, .altsetting = comm },
    { .num_altsetting = 1, .altsetting = stream },
};
static const struct usb_config_descriptor config = {
    .bLength = USB_DT_CONFIGURATION_SIZE, .bDescriptorType = USB_DT_CONFIGURATION,
    .bNumInterfaces = 2, .bConfigurationValue = 1, .bmAttributes = 0x80,
    .bMaxPower = 50, .interface = interfaces,
};
static const char *strings[] = { "CarHUB prototype", "CarHUB C6 CDC", serial };
static struct usb_cdc_line_coding coding = { .dwDTERate = 115200, .bDataBits = 8 };

static enum usbd_request_return_codes control_request(usbd_device *dev,
    struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
    void (**complete)(usbd_device *, struct usb_setup_data *)) {
    (void)dev; (void)complete;
    if (req->wIndex != 0) return USBD_REQ_NOTSUPP;
    switch (req->bRequest) {
    case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
        if (!(req->wValue & 1)) app_usb_reset();
        return USBD_REQ_HANDLED;
    case USB_CDC_REQ_SET_LINE_CODING:
        if (*len != sizeof(coding)) return USBD_REQ_NOTSUPP;
        memcpy(&coding, *buf, sizeof(coding));
        return USBD_REQ_HANDLED;
    case USB_CDC_REQ_GET_LINE_CODING:
        *buf = (uint8_t *)&coding; *len = sizeof(coding);
        return USBD_REQ_HANDLED;
    default: return USBD_REQ_NOTSUPP;
    }
}
static void receive(usbd_device *dev, uint8_t ep) {
    char bytes[64];
    int length = usbd_ep_read_packet(dev, ep, bytes, sizeof(bytes));
    for (int i = 0; i < length; ++i) app_receive(bytes[i]);
}
static void set_config(usbd_device *dev, uint16_t value) {
    configured = value == 1;
    head = tail = count = 0;
    app_usb_reset();
    if (!configured) return;
    usbd_ep_setup(dev, 0x01, USB_ENDPOINT_ATTR_BULK, 64, receive);
    usbd_ep_setup(dev, 0x82, USB_ENDPOINT_ATTR_BULK, 64, 0);
    usbd_ep_setup(dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, 0);
    usbd_register_control_callback(dev, USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
        USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT, control_request);
}
static void reset(void) {
    configured = false; head = tail = count = 0; app_usb_reset();
}
void usb_link_init(void) {
    rcc_periph_clock_enable(RCC_USB);
    desig_get_unique_id_as_string(serial, sizeof(serial));
    device = usbd_init(&st_usbfs_v1_usb_driver, &descriptor, &config,
        strings, 3, control, sizeof(control));
    usbd_register_set_config_callback(device, set_config);
    usbd_register_reset_callback(device, reset);
}
bool usb_link_configured(void) { return configured; }
uint32_t usb_link_dropped(void) { return dropped; }
bool usb_link_send(const char *line) {
    if (!configured) return false;
    unsigned length = strlen(line);
    if (length > sizeof(tx) - count) { ++dropped; return false; }
    for (unsigned i = 0; i < length; ++i) { tx[tail] = line[i]; tail = (tail + 1) % sizeof(tx); }
    count += length;
    return true;
}
void usb_link_poll(void) {
    usbd_poll(device);
    if (!configured || !count) return;
    uint8_t packet[64];
    unsigned size = count < sizeof(packet) ? count : sizeof(packet);
    for (unsigned i = 0; i < size; ++i) packet[i] = tx[(head + i) % sizeof(tx)];
    int written = usbd_ep_write_packet(device, 0x82, packet, size);
    if (written > 0) { head = (head + (unsigned)written) % sizeof(tx); count -= (unsigned)written; }
}
