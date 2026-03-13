/*
 * NEXUS OS - USB Manager
 * drivers/usb/usb_manager.c
 *
 * USB device enumeration, HID, storage, audio support
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         USB CONFIGURATION                                 */
/*===========================================================================*/

#define USB_MAX_DEVICES           128
#define USB_MAX_PORTS             16
#define USB_MAX_INTERFACES        32
#define USB_MAX_ENDPOINTS         64
#define USB_MAX_DRIVERS           16

/*===========================================================================*/
/*                         USB CONSTANTS                                     */
/*===========================================================================*/

/* USB Request Types */
#define USB_REQ_GET_STATUS        0x00
#define USB_REQ_CLEAR_FEATURE     0x01
#define USB_REQ_SET_FEATURE       0x03
#define USB_REQ_SET_ADDRESS       0x05
#define USB_REQ_GET_DESCRIPTOR    0x06
#define USB_REQ_SET_DESCRIPTOR    0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE     0x0A
#define USB_REQ_SET_INTERFACE     0x0B

/* USB Descriptor Types */
#define USB_DESC_DEVICE           0x01
#define USB_DESC_CONFIG           0x02
#define USB_DESC_STRING           0x03
#define USB_DESC_INTERFACE        0x04
#define USB_DESC_ENDPOINT         0x05
#define USB_DESC_DEVICE_QUAL      0x06
#define USB_DESC_OTHER_SPEED      0x07
#define USB_DESC_INTERFACE_POWER  0x08
#define USB_DESC_OTG              0x09
#define USB_DESC_DEBUG            0x0A
#define USB_DESC_INTERFACE_ASSOC  0x0B

/* USB Device Classes */
#define USB_CLASS_PER_INTERFACE   0x00
#define USB_CLASS_AUDIO           0x01
#define USB_CLASS_COMM            0x02
#define USB_CLASS_HID             0x03
#define USB_CLASS_PHYSICAL        0x05
#define USB_CLASS_IMAGE           0x06
#define USB_CLASS_PRINTER         0x07
#define USB_CLASS_MASS_STORAGE    0x08
#define USB_CLASS_HUB             0x09
#define USB_CLASS_DATA            0x0A
#define USB_CLASS_SMART_CARD      0x0B
#define USB_CLASS_SECURITY        0x0D
#define USB_CLASS_VIDEO           0x0E
#define USB_CLASS_HEALTHCARE      0x0F
#define USB_CLASS_AUDIO_VIDEO     0x10
#define USB_CLASS_BILLBOARD       0x11
#define USB_CLASS_USB_TYPE_C      0x12
#define USB_CLASS_MISC            0xEF
#define USB_CLASS_WIRELESS        0xE0
#define USB_CLASS_APPLICATION     0xFE
#define USB_CLASS_VENDOR_SPEC     0xFF

/* USB Speed */
#define USB_SPEED_LOW             1
#define USB_SPEED_FULL            2
#define USB_SPEED_HIGH            3
#define USB_SPEED_SUPER           4

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct __packed {
    u8 bLength;
    u8 bDescriptorType;
    u16 bcdUSB;
    u8 bDeviceClass;
    u8 bDeviceSubClass;
    u8 bDeviceProtocol;
    u8 bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8 iManufacturer;
    u8 iProduct;
    u8 iSerialNumber;
    u8 bNumConfigurations;
} usb_device_desc_t;

typedef struct __packed {
    u8 bLength;
    u8 bDescriptorType;
    u16 wTotalLength;
    u8 bNumInterfaces;
    u8 bConfigurationValue;
    u8 iConfiguration;
    u8 bmAttributes;
    u8 bMaxPower;
} usb_config_desc_t;

typedef struct __packed {
    u8 bLength;
    u8 bDescriptorType;
    u8 bInterfaceNumber;
    u8 bAlternateSetting;
    u8 bNumEndpoints;
    u8 bInterfaceClass;
    u8 bInterfaceSubClass;
    u8 bInterfaceProtocol;
    u8 iInterface;
} usb_interface_desc_t;

typedef struct __packed {
    u8 bLength;
    u8 bDescriptorType;
    u8 bEndpointAddress;
    u8 bmAttributes;
    u16 wMaxPacketSize;
    u8 bInterval;
} usb_endpoint_desc_t;

typedef struct {
    u32 device_id;
    u32 port;
    u32 address;
    u32 speed;
    usb_device_desc_t device_desc;
    usb_config_desc_t config_desc;
    char manufacturer[64];
    char product[64];
    char serial[64];
    bool is_configured;
    bool is_suspended;
    void *driver_data;
} usb_device_t;

typedef struct {
    bool initialized;
    usb_device_t devices[USB_MAX_DEVICES];
    u32 device_count;
    u32 root_hub_ports;
    void (*on_device_connect)(usb_device_t *);
    void (*on_device_disconnect)(usb_device_t *);
} usb_manager_t;

static usb_manager_t g_usb;

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int usb_init(void)
{
    printk("[USB] ========================================\n");
    printk("[USB] NEXUS OS USB Manager\n");
    printk("[USB] ========================================\n");
    
    memset(&g_usb, 0, sizeof(usb_manager_t));
    g_usb.initialized = true;
    g_usb.root_hub_ports = 8;  /* Typical root hub */
    
    printk("[USB] USB Manager initialized (%d ports)\n", g_usb.root_hub_ports);
    return 0;
}

int usb_shutdown(void)
{
    printk("[USB] Shutting down USB Manager...\n");
    
    /* Reset all devices */
    for (u32 i = 0; i < g_usb.device_count; i++) {
        usb_device_t *dev = &g_usb.devices[i];
        if (dev->is_configured) {
            /* Reset device */
        }
    }
    
    g_usb.initialized = false;
    return 0;
}

/*===========================================================================*/
/*                         DEVICE ENUMERATION                                */
/*===========================================================================*/

static int usb_get_device_descriptor(usb_device_t *dev)
{
    /* In real implementation, would read via control transfer */
    /* Mock descriptor */
    dev->device_desc.bLength = 18;
    dev->device_desc.bDescriptorType = USB_DESC_DEVICE;
    dev->device_desc.bcdUSB = 0x0200;  /* USB 2.0 */
    dev->device_desc.bMaxPacketSize0 = 64;
    dev->device_desc.idVendor = 0x8087;  /* Intel */
    dev->device_desc.idProduct = 0x0024;
    dev->device_desc.bcdDevice = 0x0000;
    dev->device_desc.bNumConfigurations = 1;
    
    return 0;
}

static int usb_get_config_descriptor(usb_device_t *dev)
{
    /* Mock config descriptor */
    dev->config_desc.bLength = 9;
    dev->config_desc.bDescriptorType = USB_DESC_CONFIG;
    dev->config_desc.wTotalLength = 32;
    dev->config_desc.bNumInterfaces = 1;
    dev->config_desc.bConfigurationValue = 1;
    dev->config_desc.bmAttributes = 0xC0;  /* Self-powered */
    dev->config_desc.bMaxPower = 50;  /* 100mA */
    
    return 0;
}

int usb_enumerate_bus(void)
{
    printk("[USB] Enumerating USB bus...\n");
    
    g_usb.device_count = 0;
    
    /* Mock discovered devices */
    /* Device 1: USB Keyboard */
    usb_device_t *kbd = &g_usb.devices[g_usb.device_count++];
    kbd->device_id = 1;
    kbd->port = 1;
    kbd->address = 1;
    kbd->speed = USB_SPEED_FULL;
    kbd->device_desc.bDeviceClass = USB_CLASS_HID;
    kbd->device_desc.idVendor = 0x046D;  /* Logitech */
    kbd->device_desc.idProduct = 0xC31C;
    strcpy(kbd->manufacturer, "Logitech");
    strcpy(kbd->product, "USB Keyboard");
    kbd->is_configured = true;
    
    /* Device 2: USB Mouse */
    usb_device_t *mouse = &g_usb.devices[g_usb.device_count++];
    mouse->device_id = 2;
    mouse->port = 2;
    mouse->address = 2;
    mouse->speed = USB_SPEED_FULL;
    mouse->device_desc.bDeviceClass = USB_CLASS_HID;
    mouse->device_desc.idVendor = 0x046D;
    mouse->device_desc.idProduct = 0xC077;
    strcpy(mouse->manufacturer, "Logitech");
    strcpy(mouse->product, "USB Mouse");
    mouse->is_configured = true;
    
    /* Device 3: USB Storage */
    usb_device_t *storage = &g_usb.devices[g_usb.device_count++];
    storage->device_id = 3;
    storage->port = 3;
    storage->address = 3;
    storage->speed = USB_SPEED_HIGH;
    storage->device_desc.bDeviceClass = USB_CLASS_MASS_STORAGE;
    storage->device_desc.idVendor = 0x0781;  /* SanDisk */
    storage->device_desc.idProduct = 0x5567;
    strcpy(storage->manufacturer, "SanDisk");
    strcpy(storage->product, "USB Flash Drive");
    storage->is_configured = true;
    
    /* Device 4: USB Audio */
    usb_device_t *audio = &g_usb.devices[g_usb.device_count++];
    audio->device_id = 4;
    audio->port = 4;
    audio->address = 4;
    audio->speed = USB_SPEED_FULL;
    audio->device_desc.bDeviceClass = USB_CLASS_AUDIO;
    audio->device_desc.idVendor = 0x046D;
    audio->device_desc.idProduct = 0x0A37;
    strcpy(audio->manufacturer, "Logitech");
    strcpy(audio->product, "USB Headset");
    audio->is_configured = true;
    
    printk("[USB] Found %d USB devices\n", g_usb.device_count);
    
    for (u32 i = 0; i < g_usb.device_count; i++) {
        printk("[USB]   [%d] %s %s (VID:0x%04X, PID:0x%04X)\n",
               i, g_usb.devices[i].manufacturer,
               g_usb.devices[i].product,
               g_usb.devices[i].device_desc.idVendor,
               g_usb.devices[i].device_desc.idProduct);
    }
    
    return g_usb.device_count;
}

usb_device_t *usb_get_device(u32 device_id)
{
    for (u32 i = 0; i < g_usb.device_count; i++) {
        if (g_usb.devices[i].device_id == device_id) {
            return &g_usb.devices[i];
        }
    }
    return NULL;
}

/*===========================================================================*/
/*                         USB HID DRIVER                                    */
/*===========================================================================*/

typedef struct {
    u8 report_id;
    u8 modifier;
    u8 reserved;
    u8 keys[6];
} usb_kbd_report_t;

typedef struct {
    u8 buttons;
    s8 x;
    s8 y;
    s8 wheel;
    s8 reserved;
} usb_mouse_report_t;

int usb_hid_read(u32 device_id, void *buffer, u32 size)
{
    usb_device_t *dev = usb_get_device(device_id);
    if (!dev || dev->device_desc.bDeviceClass != USB_CLASS_HID) {
        return -EINVAL;
    }
    
    /* In real implementation, would read via interrupt transfer */
    /* Mock: return placeholder data */
    if (size >= 8) {
        memset(buffer, 0, size);
    }
    
    return size;
}

int usb_hid_write(u32 device_id, const void *buffer, u32 size)
{
    usb_device_t *dev = usb_get_device(device_id);
    if (!dev || dev->device_desc.bDeviceClass != USB_CLASS_HID) {
        return -EINVAL;
    }
    
    /* In real implementation, would write via interrupt transfer */
    return size;
}

/*===========================================================================*/
/*                         USB MASS STORAGE                                  */
/*===========================================================================*/

#define USB_MSC_CBW_SIGNATURE   0x43425355
#define USB_MSC_CSW_SIGNATURE   0x53425355

typedef struct __packed {
    u32 signature;
    u32 tag;
    u32 data_transfer_length;
    u8 flags;
    u8 lun;
    u8 cb_length;
    u8 cb[16];
} usb_msc_cbw_t;

typedef struct __packed {
    u32 signature;
    u32 tag;
    u32 data_residual;
    u8 status;
} usb_msc_csw_t;

int usb_msc_read(u32 device_id, u32 lba, void *buffer, u32 sectors)
{
    usb_device_t *dev = usb_get_device(device_id);
    if (!dev || dev->device_desc.bDeviceClass != USB_CLASS_MASS_STORAGE) {
        return -EINVAL;
    }
    
    /* In real implementation, would send SCSI READ(10) command */
    printk("[USB_MSC] Read LBA %d, %d sectors from device %d\n", 
           lba, sectors, device_id);
    
    /* Mock: fill with zeros */
    memset(buffer, 0, sectors * 512);
    
    return 0;
}

int usb_msc_write(u32 device_id, u32 lba, const void *buffer, u32 sectors)
{
    usb_device_t *dev = usb_get_device(device_id);
    if (!dev || dev->device_desc.bDeviceClass != USB_CLASS_MASS_STORAGE) {
        return -EINVAL;
    }
    
    /* In real implementation, would send SCSI WRITE(10) command */
    printk("[USB_MSC] Write LBA %d, %d sectors to device %d\n",
           lba, sectors, device_id);
    
    return 0;
}

int usb_msc_get_capacity(u32 device_id, u32 *max_lba, u32 *block_size)
{
    usb_device_t *dev = usb_get_device(device_id);
    if (!dev) return -EINVAL;
    
    /* Mock capacity: 8GB */
    *max_lba = 16777215;  /* 8GB / 512 - 1 */
    *block_size = 512;
    
    return 0;
}

/*===========================================================================*/
/*                         USB AUDIO                                         */
/*===========================================================================*/

int usb_audio_set_format(u32 device_id, u32 sample_rate, u32 channels, u32 bits)
{
    usb_device_t *dev = usb_get_device(device_id);
    if (!dev || dev->device_desc.bDeviceClass != USB_CLASS_AUDIO) {
        return -EINVAL;
    }
    
    printk("[USB_AUDIO] Set format: %dHz, %d ch, %d bits\n",
           sample_rate, channels, bits);
    
    /* In real implementation, would send SET_CUR request */
    return 0;
}

int usb_audio_stream(u32 device_id, void *buffer, u32 size, bool write)
{
    usb_device_t *dev = usb_get_device(device_id);
    if (!dev || dev->device_desc.bDeviceClass != USB_CLASS_AUDIO) {
        return -EINVAL;
    }
    
    /* In real implementation, would use isochronous transfer */
    if (write) {
        /* Playback */
        memset(buffer, 0, size);  /* Mock: silence */
    } else {
        /* Capture */
    }
    
    return size;
}

/*===========================================================================*/
/*                         HOTPLUG                                           */
/*===========================================================================*/

int usb_set_hotplug_callback(void (*connect)(usb_device_t *),
                             void (*disconnect)(usb_device_t *))
{
    g_usb.on_device_connect = connect;
    g_usb.on_device_disconnect = disconnect;
    return 0;
}

static void usb_check_hotplug(void)
{
    /* In real implementation, would check root hub status */
    /* Mock: assume no changes */
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

int usb_list_devices(usb_device_t *devices, u32 *count)
{
    if (!devices || !count) return -EINVAL;
    
    u32 copy = (*count < g_usb.device_count) ? *count : g_usb.device_count;
    memcpy(devices, g_usb.devices, sizeof(usb_device_t) * copy);
    *count = copy;
    
    return 0;
}

const char *usb_get_class_name(u8 class)
{
    switch (class) {
        case USB_CLASS_AUDIO:         return "Audio";
        case USB_CLASS_COMM:          return "Communications";
        case USB_CLASS_HID:           return "HID";
        case USB_CLASS_IMAGE:         return "Imaging";
        case USB_CLASS_PRINTER:       return "Printer";
        case USB_CLASS_MASS_STORAGE:  return "Mass Storage";
        case USB_CLASS_HUB:           return "Hub";
        case USB_CLASS_VIDEO:         return "Video";
        case USB_CLASS_WIRELESS:      return "Wireless";
        default:                      return "Unknown";
    }
}

const char *usb_get_speed_name(u32 speed)
{
    switch (speed) {
        case USB_SPEED_LOW:   return "Low Speed (1.5 Mbps)";
        case USB_SPEED_FULL:  return "Full Speed (12 Mbps)";
        case USB_SPEED_HIGH:  return "High Speed (480 Mbps)";
        case USB_SPEED_SUPER: return "Super Speed (5 Gbps)";
        default:              return "Unknown";
    }
}

usb_manager_t *usb_get_manager(void)
{
    return &g_usb;
}
