#include "hid_switch.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include <lib/libusb_stm32/inc/usb.h>
#include <lib/libusb_stm32/inc/usbd_core.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TAG "HIDSwitch"

#define HID_EP_IN     0x81
#define HID_EP_IN_IDX 0x01
#define HID_EP_SIZE   16

// Switch Pro Controller HID Report Descriptor
static const uint8_t switch_report_desc[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x05,        // Usage (Gamepad)
    0xA1, 0x01,        // Collection (Application)
    // Buttons 1-8
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x08,        //   Usage Maximum (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    // Buttons 9-16
    0x95, 0x08,        //   Report Count (8)
    0x19, 0x09,        //   Usage Minimum (9)
    0x29, 0x10,        //   Usage Maximum (16)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    // HAT switch
    0x05, 0x01,        //   Usage Page (Generic Desktop)
    0x25, 0x07,        //   Logical Maximum (7)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x65, 0x14,        //   Unit (Degrees)
    0x09, 0x39,        //   Usage (Hat Switch)
    0x81, 0x42,        //   Input (Variable, Null State)
    // Padding
    0x65, 0x00,
    0x75, 0x04,
    0x95, 0x01,
    0x81, 0x03,        //   Input (Constant)
    // Left stick X/Y
    0x26, 0xFF, 0x0F,
    0x46, 0xFF, 0x0F,
    0x75, 0x10,
    0x95, 0x02,
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x81, 0x02,
    // Right stick X/Y
    0x95, 0x02,
    0x09, 0x33,        //   Usage (Rx)
    0x09, 0x34,        //   Usage (Ry)
    0x81, 0x02,
    0xC0               // End Collection
};

// Device descriptor
static struct usb_device_descriptor switch_device_desc = {
    .bLength            = sizeof(struct usb_device_descriptor),
    .bDescriptorType    = USB_DTYPE_DEVICE,
    .bcdUSB             = VERSION_BCD(2, 0, 0),
    .bDeviceClass       = USB_CLASS_PER_INTERFACE,
    .bDeviceSubClass    = USB_SUBCLASS_NONE,
    .bDeviceProtocol    = USB_PROTO_NONE,
    .bMaxPacketSize0    = 8,
    .idVendor           = SWITCH_VID,
    .idProduct          = SWITCH_PID,
    .bcdDevice          = VERSION_BCD(1, 0, 0),
    .iManufacturer      = 1,
    .iProduct           = 2,
    .iSerialNumber      = NO_DESCRIPTOR,
    .bNumConfigurations = 1,
};

// Config descriptor as raw bytes to avoid struct issues
typedef struct {
    struct usb_config_descriptor    config;
    struct usb_interface_descriptor iface;
    uint8_t                         hid_desc[9];
    struct usb_endpoint_descriptor  ep_in;
} __attribute__((packed)) SwitchConfigDesc;

static const SwitchConfigDesc switch_config_desc = {
    .config = {
        .bLength             = sizeof(struct usb_config_descriptor),
        .bDescriptorType     = USB_DTYPE_CONFIGURATION,
        .wTotalLength        = sizeof(SwitchConfigDesc),
        .bNumInterfaces      = 1,
        .bConfigurationValue = 1,
        .iConfiguration      = NO_DESCRIPTOR,
        .bmAttributes        = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
        .bMaxPower           = USB_CFG_POWER_MA(100),
    },
    .iface = {
        .bLength             = sizeof(struct usb_interface_descriptor),
        .bDescriptorType     = USB_DTYPE_INTERFACE,
        .bInterfaceNumber    = 0,
        .bAlternateSetting   = 0,
        .bNumEndpoints       = 1,
        .bInterfaceClass     = 0x03,
        .bInterfaceSubClass  = 0x00,
        .bInterfaceProtocol  = 0x00,
        .iInterface          = NO_DESCRIPTOR,
    },
    .hid_desc = {
        0x09,        // bLength
        0x21,        // bDescriptorType (HID)
        0x11, 0x01,  // bcdHID 1.11
        0x00,        // bCountryCode
        0x01,        // bNumDescriptors
        0x22,        // bDescriptorType (Report)
        sizeof(switch_report_desc) & 0xFF,
        (sizeof(switch_report_desc) >> 8) & 0xFF,
    },
    .ep_in = {
        .bLength             = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType     = USB_DTYPE_ENDPOINT,
        .bEndpointAddress    = HID_EP_IN,
        .bmAttributes        = USB_EPTYPE_INTERRUPT,
        .wMaxPacketSize      = HID_EP_SIZE,
        .bInterval           = 8,
    },
};

// String descriptors
static const struct usb_string_descriptor lang_desc =
    USB_ARRAY_DESC(USB_LANGID_ENG_US);
static const struct usb_string_descriptor manuf_desc =
    USB_STRING_DESC("Nintendo");
static const struct usb_string_descriptor prod_desc =
    USB_STRING_DESC("Pro Controller");

// State
static usbd_device*         switch_usb_dev   = NULL;
static FuriHalUsbInterface* saved_usb_mode   = NULL;

// Forward declarations
static void switch_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
static void switch_usb_deinit(usbd_device* dev);
static usbd_respond switch_usb_descriptor(usbd_ctlreq* req, void** address, uint16_t* dsize);
static usbd_respond switch_usb_config(usbd_device* dev, uint8_t cfg);
static usbd_respond switch_usb_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback);

// USB interface profile
static FuriHalUsbInterface switch_usb_interface = {
    .init             = switch_usb_init,
    .deinit           = switch_usb_deinit,
    .wakeup           = NULL,
    .suspend          = NULL,
    .dev_descr        = &switch_device_desc,
    .str_manuf_descr  = (void*)&manuf_desc,
    .str_prod_descr   = (void*)&prod_desc,
    .str_serial_descr = NULL,
    .cfg_descr        = (void*)&switch_config_desc,
};

static usbd_respond switch_usb_descriptor(usbd_ctlreq* req, void** address, uint16_t* dsize) {
    uint8_t dtype   = req->wValue >> 8;
    uint8_t dnumber = req->wValue & 0xFF;

    switch(dtype) {
    case USB_DTYPE_DEVICE:
        *address = &switch_device_desc;
        *dsize   = sizeof(switch_device_desc);
        return usbd_ack;
    case USB_DTYPE_CONFIGURATION:
        *address = (void*)&switch_config_desc;
        *dsize   = sizeof(switch_config_desc);
        return usbd_ack;
    case USB_DTYPE_STRING:
        if(dnumber == 0) {
            *address = (void*)&lang_desc;
            *dsize   = lang_desc.bLength;
        } else if(dnumber == 1) {
            *address = (void*)&manuf_desc;
            *dsize   = manuf_desc.bLength;
        } else if(dnumber == 2) {
            *address = (void*)&prod_desc;
            *dsize   = prod_desc.bLength;
        } else {
            return usbd_fail;
        }
        return usbd_ack;
    case 0x21: // HID descriptor
        *address = (void*)switch_config_desc.hid_desc;
        *dsize   = sizeof(switch_config_desc.hid_desc);
        return usbd_ack;
    case 0x22: // HID Report descriptor
        *address = (void*)switch_report_desc;
        *dsize   = sizeof(switch_report_desc);
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

static usbd_respond switch_usb_config(usbd_device* dev, uint8_t cfg) {
    if(cfg == 0) {
        usbd_ep_deconfig(dev, HID_EP_IN);
        return usbd_ack;
    } else if(cfg == 1) {
        usbd_ep_config(dev, HID_EP_IN, USB_EPTYPE_INTERRUPT, HID_EP_SIZE);
        return usbd_ack;
    }
    return usbd_fail;
}

static usbd_respond switch_usb_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(dev);
    UNUSED(callback);
    if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_CLASS) {
        switch(req->bRequest) {
        case 0x0A: // SET_IDLE
            return usbd_ack;
        case 0x01: // GET_REPORT
            return usbd_ack;
        default:
            return usbd_fail;
        }
    }
    return usbd_fail;
}

static void switch_usb_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    UNUSED(ctx);
    switch_usb_dev = dev;
    usbd_reg_descr(dev, switch_usb_descriptor);
    usbd_reg_config(dev, switch_usb_config);
    usbd_reg_control(dev, switch_usb_control);
    FURI_LOG_I(TAG, "Switch HID USB init");
}

static void switch_usb_deinit(usbd_device* dev) {
    UNUSED(dev);
    switch_usb_dev = NULL;
    FURI_LOG_I(TAG, "Switch HID USB deinit");
}

bool hid_switch_init(void) {
    FURI_LOG_I(TAG, "Starting Switch Pro Controller HID");
    saved_usb_mode = furi_hal_usb_get_config();
    if(!furi_hal_usb_set_config(&switch_usb_interface, NULL)) {
        FURI_LOG_E(TAG, "Failed to set USB config");
        return false;
    }
    furi_delay_ms(500);
    FURI_LOG_I(TAG, "Switch HID ready");
    return true;
}

void hid_switch_deinit(void) {
    FURI_LOG_I(TAG, "Restoring USB mode");
    if(saved_usb_mode) {
        furi_hal_usb_set_config(saved_usb_mode, NULL);
        saved_usb_mode = NULL;
    }
    switch_usb_dev = NULL;
}

void hid_switch_send_report(const SwitchReport* report) {
    if(!switch_usb_dev) {
        FURI_LOG_W(TAG, "USB not ready");
        return;
    }
    usbd_ep_write(switch_usb_dev, HID_EP_IN_IDX, (void*)report, sizeof(SwitchReport));
}