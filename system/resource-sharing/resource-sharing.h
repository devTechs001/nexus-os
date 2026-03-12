/*
 * NEXUS OS - Resource Sharing Framework
 * system/resource-sharing/resource-sharing.h
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * Provides seamless integration with host system for:
 * - Shared folders
 * - Shared network
 * - Shared devices (USB, etc.)
 * - Direct hardware access (native mode)
 * - Mode-specific optimizations
 */

#ifndef _NEXUS_RESOURCE_SHARING_H
#define _NEXUS_RESOURCE_SHARING_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         SHARING MODES                                     */
/*===========================================================================*/

/* Virtualization Modes */
#define MODE_VMWARE             0
#define MODE_VIRTUALBOX         1
#define MODE_QEMU               2
#define MODE_HYPERV             3
#define MODE_NATIVE             4
#define MODE_CONTAINER          5

/* Sharing Levels */
#define SHARING_NONE            0
#define SHARING_BASIC           1
#define SHARING_ENHANCED        2
#define SHARING_FULL            3

/* Resource Types */
#define RESOURCE_FOLDER         0x01
#define RESOURCE_NETWORK        0x02
#define RESOURCE_USB            0x04
#define RESOURCE_AUDIO          0x08
#define RESOURCE_PRINTER        0x10
#define RESOURCE_CLIPBOARD      0x20
#define RESOURCE_DRAGDROP       0x40
#define RESOURCE_DISPLAY        0x80

/*===========================================================================*/
/*                         SHARED FOLDER STRUCTURES                          */
/*===========================================================================*/

/**
 * shared_folder_t - Shared folder configuration
 */
typedef struct shared_folder {
    char name[256];
    char host_path[512];
    char guest_path[512];
    bool enabled;
    bool readonly;
    bool auto_mount;
    u32 flags;
    u64 size_used;
    u64 size_total;
    struct shared_folder *next;
} shared_folder_t;

/**
 * folder_share_config_t - Folder sharing configuration
 */
typedef struct {
    shared_folder_t *folders;
    int folder_count;
    bool enabled;
    char default_guest_path[512];
    u32 default_permissions;
} folder_share_config_t;

/*===========================================================================*/
/*                         NETWORK SHARING STRUCTURES                        */
/*===========================================================================*/

/**
 * network_share_t - Network sharing configuration
 */
typedef struct {
    bool enabled;
    bool bridge_mode;
    bool nat_mode;
    bool host_only;
    char bridge_interface[64];
    char nat_network[64];
    bool port_forwarding;
    
    struct {
        char host_port[16];
        char guest_port[16];
        char protocol[8];
    } forwards[32];
    int forward_count;
    
    bool shared_internet;
    bool dns_proxy;
} network_share_config_t;

/*===========================================================================*/
/*                         DEVICE SHARING STRUCTURES                         */
/*===========================================================================*/

/**
 * usb_share_t - USB device sharing
 */
typedef struct {
    bool enabled;
    bool auto_connect;
    u16 vendor_id[32];
    u16 product_id[32];
    int device_count;
    bool share_all;
} usb_share_config_t;

/**
 * device_share_t - Device sharing configuration
 */
typedef struct {
    usb_share_config_t usb;
    
    bool share_audio;
    bool share_printer;
    bool share_webcam;
    bool share_bluetooth;
    bool share_smartcard;
    
    /* Direct hardware access (native mode) */
    bool direct_disk_access;
    bool direct_gpu_access;
    bool direct_network_access;
} device_share_config_t;

/*===========================================================================*/
/*                         CLIPBOARD & DRAG-DROP                             */
/*===========================================================================*/

/**
 * clipboard_config_t - Clipboard sharing
 */
typedef struct {
    bool enabled;
    bool host_to_guest;
    bool guest_to_host;
    bool support_text;
    bool support_files;
    bool support_images;
    size_t max_size;
} clipboard_config_t;

/**
 * dragdrop_config_t - Drag and drop sharing
 */
typedef struct {
    bool enabled;
    bool host_to_guest;
    bool guest_to_host;
    bool support_files;
    bool support_text;
    char temp_path[512];
} dragdrop_config_t;

/*===========================================================================*/
/*                         DISPLAY SHARING                                   */
/*===========================================================================*/

/**
 * display_config_t - Display sharing
 */
typedef struct {
    bool enabled;
    bool auto_resize;
    bool seamless_mode;
    bool unity_mode;
    bool multi_monitor;
    u32 max_resolution;
    u32 color_depth;
    bool acceleration_3d;
    bool acceleration_2d;
    u32 video_memory_mb;
} display_config_t;

/*===========================================================================*/
/*                         MAIN CONFIGURATION                                */
/*===========================================================================*/

/**
 * resource_sharing_config_t - Main resource sharing configuration
 */
typedef struct {
    /* Mode detection */
    int virtualization_mode;
    int sharing_level;
    bool auto_detect;
    
    /* Resource configurations */
    folder_share_config_t folders;
    network_share_config_t network;
    device_share_config_t devices;
    clipboard_config_t clipboard;
    dragdrop_config_t dragdrop;
    display_config_t display;
    
    /* Performance */
    bool enable_caching;
    bool enable_compression;
    u32 cache_size_mb;
    u32 max_transfer_rate;
    
    /* Security */
    bool verify_host;
    bool encrypt_transfers;
    bool sandbox_shared;
    
    /* Statistics */
    struct {
        u64 folders_accessed;
        u64 bytes_transferred;
        u64 usb_devices_connected;
        u64 clipboard_operations;
        u64 dragdrop_operations;
    } stats;
} resource_sharing_config_t;

/*===========================================================================*/
/*                         RESOURCE SHARING API                              */
/*===========================================================================*/

/* Initialization */
int resource_sharing_init(resource_sharing_config_t *config);
int resource_sharing_shutdown(resource_sharing_config_t *config);
int resource_sharing_detect_mode(void);

/* Mode-specific initialization */
int vmware_init_sharing(resource_sharing_config_t *config);
int virtualbox_init_sharing(resource_sharing_config_t *config);
int qemu_init_sharing(resource_sharing_config_t *config);
int hyperv_init_sharing(resource_sharing_config_t *config);
int native_init_sharing(resource_sharing_config_t *config);

/* Folder sharing */
int shared_folder_add(resource_sharing_config_t *config, const char *name,
                      const char *host_path, const char *guest_path);
int shared_folder_remove(resource_sharing_config_t *config, const char *name);
int shared_folder_mount(resource_sharing_config_t *config, const char *name);
int shared_folder_unmount(resource_sharing_config_t *config, const char *name);
int shared_folder_list(resource_sharing_config_t *config);

/* Network sharing */
int network_share_enable(resource_sharing_config_t *config);
int network_share_disable(resource_sharing_config_t *config);
int network_bridge_create(resource_sharing_config_t *config);
int network_port_forward(resource_sharing_config_t *config, 
                         const char *host_port, const char *guest_port,
                         const char *protocol);

/* Device sharing */
int usb_device_share(resource_sharing_config_t *config, 
                     u16 vendor_id, u16 product_id);
int usb_device_unshare(resource_sharing_config_t *config,
                       u16 vendor_id, u16 product_id);
int usb_device_connect(resource_sharing_config_t *config,
                       u16 vendor_id, u16 product_id);
int usb_device_disconnect(resource_sharing_config_t *config,
                          u16 vendor_id, u16 product_id);

/* Clipboard */
int clipboard_share_enable(resource_sharing_config_t *config);
int clipboard_share_disable(resource_sharing_config_t *config);
int clipboard_set_text(const char *text);
char *clipboard_get_text(void);

/* Drag and Drop */
int dragdrop_enable(resource_sharing_config_t *config);
int dragdrop_disable(resource_sharing_config_t *config);

/* Direct hardware access (native mode) */
int direct_disk_access_enable(const char *device_path);
int direct_gpu_access_enable(void);
int direct_network_access_enable(const char *interface);

/* Performance */
int resource_sharing_set_level(resource_sharing_config_t *config, int level);
int resource_sharing_optimize(resource_sharing_config_t *config);

/* Statistics */
int resource_sharing_get_stats(resource_sharing_config_t *config, void *stats);
int resource_sharing_reset_stats(resource_sharing_config_t *config);

/* Configuration */
int resource_sharing_load_config(resource_sharing_config_t *config, 
                                  const char *path);
int resource_sharing_save_config(resource_sharing_config_t *config,
                                  const char *path);
int resource_sharing_reset_to_defaults(resource_sharing_config_t *config);

/* Auto-detection and setup */
int resource_sharing_auto_configure(resource_sharing_config_t *config);
int resource_sharing_quick_setup(resource_sharing_config_t *config, int mode);

#endif /* _NEXUS_RESOURCE_SHARING_H */
