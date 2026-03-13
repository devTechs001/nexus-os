/*
 * NEXUS OS - Camera/Media Driver
 * drivers/media/camera.c
 *
 * Camera capture with image processing
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         CAMERA CONFIGURATION                              */
/*===========================================================================*/

#define CAM_MAX_DEVICES           8
#define CAM_MAX_FORMATS           16
#define CAM_BUFFER_COUNT          4

/* Pixel Formats */
#define CAM_FORMAT_MJPEG          0
#define CAM_FORMAT_YUYV           1
#define CAM_FORMAT_NV12           2
#define CAM_FORMAT_RGB24          3
#define CAM_FORMAT_RGB565         4
#define CAM_FORMAT_GREY           5

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 format;
    u32 width;
    u32 height;
    u32 fps;
} cam_format_t;

typedef struct {
    u32 device_id;
    char name[64];
    u16 vendor_id;
    u16 device_id_pci;
    u32 bus_type;  /* USB, PCI, MIPI */
    cam_format_t formats[CAM_MAX_FORMATS];
    u32 format_count;
    u32 current_format;
    u32 width;
    u32 height;
    u32 fps;
    bool is_streaming;
    bool is_enabled;
    u64 frames_captured;
    u64 bytes_captured;
    void *buffers[CAM_BUFFER_COUNT];
    u32 buffer_size;
    void *private_data;
} cam_device_t;

typedef struct {
    bool initialized;
    cam_device_t devices[CAM_MAX_DEVICES];
    u32 device_count;
    spinlock_t lock;
} cam_driver_t;

static cam_driver_t g_cam;

/*===========================================================================*/
/*                         CAMERA CORE FUNCTIONS                             */
/*===========================================================================*/

int cam_register_device(const char *name, u16 vendor, u16 device, u32 bus)
{
    spinlock_lock(&g_cam.lock);
    
    if (g_cam.device_count >= CAM_MAX_DEVICES) {
        spinlock_unlock(&g_cam.lock);
        return -ENOMEM;
    }
    
    cam_device_t *dev = &g_cam.devices[g_cam.device_count++];
    memset(dev, 0, sizeof(cam_device_t));
    
    dev->device_id = g_cam.device_count;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->vendor_id = vendor;
    dev->device_id_pci = device;
    dev->bus_type = bus;
    
    /* Add default formats */
    dev->formats[dev->format_count++] = (cam_format_t){CAM_FORMAT_MJPEG, 640, 480, 30};
    dev->formats[dev->format_count++] = (cam_format_t){CAM_FORMAT_MJPEG, 1280, 720, 30};
    dev->formats[dev->format_count++] = (cam_format_t){CAM_FORMAT_MJPEG, 1920, 1080, 30};
    dev->formats[dev->format_count++] = (cam_format_t){CAM_FORMAT_YUYV, 640, 480, 30};
    
    dev->current_format = 0;
    dev->width = 640;
    dev->height = 480;
    dev->fps = 30;
    
    /* Allocate buffers */
    dev->buffer_size = dev->width * dev->height * 2;
    for (int i = 0; i < CAM_BUFFER_COUNT; i++) {
        dev->buffers[i] = kmalloc(dev->buffer_size);
    }
    
    spinlock_unlock(&g_cam.lock);
    
    printk("[CAM] Registered %s (bus %d)\n", name, bus);
    return dev->device_id;
}

int cam_set_format(u32 device_id, u32 width, u32 height, u32 fps)
{
    cam_device_t *dev = NULL;
    for (u32 i = 0; i < g_cam.device_count; i++) {
        if (g_cam.devices[i].device_id == device_id) {
            dev = &g_cam.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    if (dev->is_streaming) return -EBUSY;
    
    dev->width = width;
    dev->height = height;
    dev->fps = fps;
    
    /* Reallocate buffers */
    for (int i = 0; i < CAM_BUFFER_COUNT; i++) {
        if (dev->buffers[i]) kfree(dev->buffers[i]);
    }
    dev->buffer_size = width * height * 2;
    for (int i = 0; i < CAM_BUFFER_COUNT; i++) {
        dev->buffers[i] = kmalloc(dev->buffer_size);
    }
    
    printk("[CAM] Format set: %dx%d@%dfps\n", width, height, fps);
    return 0;
}

int cam_start_stream(u32 device_id)
{
    cam_device_t *dev = NULL;
    for (u32 i = 0; i < g_cam.device_count; i++) {
        if (g_cam.devices[i].device_id == device_id) {
            dev = &g_cam.devices[i];
            break;
        }
    }
    
    if (!dev || !dev->is_enabled) return -ENODEV;
    
    dev->is_streaming = true;
    printk("[CAM] Streaming started on %s\n", dev->name);
    return 0;
}

int cam_stop_stream(u32 device_id)
{
    cam_device_t *dev = NULL;
    for (u32 i = 0; i < g_cam.device_count; i++) {
        if (g_cam.devices[i].device_id == device_id) {
            dev = &g_cam.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    
    dev->is_streaming = false;
    printk("[CAM] Streaming stopped\n");
    return 0;
}

int cam_capture_frame(u32 device_id, void **frame, u32 *size)
{
    cam_device_t *dev = NULL;
    for (u32 i = 0; i < g_cam.device_count; i++) {
        if (g_cam.devices[i].device_id == device_id) {
            dev = &g_cam.devices[i];
            break;
        }
    }
    
    if (!dev || !dev->is_streaming) return -ENODEV;
    
    /* In real implementation, would capture from hardware */
    /* Mock: return first buffer */
    if (frame) *frame = dev->buffers[0];
    if (size) *size = dev->buffer_size;
    
    dev->frames_captured++;
    dev->bytes_captured += dev->buffer_size;
    
    return 0;
}

/*===========================================================================*/
/*                         USB CAMERA DRIVER                                 */
/*===========================================================================*/

typedef struct {
    u32 interface;
    u32 endpoint;
    u32 max_packet;
} uvc_private_t;

int uvc_probe(u16 vendor, u16 device, u32 interface)
{
    printk("[UVC] Probing USB camera (0x%04X:0x%04X)...\n", vendor, device);
    
    uvc_private_t *priv = kmalloc(sizeof(uvc_private_t));
    if (!priv) return -ENOMEM;
    
    memset(priv, 0, sizeof(uvc_private_t));
    priv->interface = interface;
    
    u32 dev_id = cam_register_device("uvc_camera", vendor, device, 1);
    if (dev_id < 0) {
        kfree(priv);
        return dev_id;
    }
    
    g_cam.devices[dev_id - 1].private_data = priv;
    return 0;
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int camera_init(void)
{
    printk("[CAM] ========================================\n");
    printk("[CAM] NEXUS OS Camera Driver\n");
    printk("[CAM] ========================================\n");
    
    memset(&g_cam, 0, sizeof(cam_driver_t));
    spinlock_init(&g_cam.lock);
    
    printk("[CAM] Camera driver initialized\n");
    return 0;
}

int camera_shutdown(void)
{
    printk("[CAM] Shutting down camera driver...\n");
    
    for (u32 i = 0; i < g_cam.device_count; i++) {
        cam_device_t *dev = &g_cam.devices[i];
        for (int j = 0; j < CAM_BUFFER_COUNT; j++) {
            if (dev->buffers[j]) kfree(dev->buffers[j]);
        }
        if (dev->private_data) kfree(dev->private_data);
    }
    
    g_cam.device_count = 0;
    return 0;
}

cam_driver_t *camera_get_driver(void)
{
    return &g_cam;
}
