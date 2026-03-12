/*
 * NEXUS OS - Storage Management UI
 * gui/storage/storage-manager.h
 *
 * Advanced storage management with real-time monitoring and SMART visualization
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _STORAGE_MANAGER_H
#define _STORAGE_MANAGER_H

#include "../gui.h"
#include "../widgets/widgets.h"
#include "../../kernel/include/kernel.h"
#include "../../kernel/include/types.h"
#include "../../drivers/storage/nvme.h"
#include "../../drivers/storage/ahci.h"

/*===========================================================================*/
/*                         STORAGE MANAGER CONFIGURATION                     */
/*===========================================================================*/

#define STORAGE_MANAGER_VERSION     "1.0.0"
#define STORAGE_MAX_DEVICES         32
#define STORAGE_MAX_PARTITIONS      256
#define STORAGE_MAX_SMART_ATTRS     64
#define STORAGE_HISTORY_SAMPLES     120
#define STORAGE_UPDATE_INTERVAL_MS  1000

/*===========================================================================*/
/*                         STORAGE DEVICE TYPES                              */
/*===========================================================================*/

#define STORAGE_TYPE_UNKNOWN        0
#define STORAGE_TYPE_NVME           1
#define STORAGE_TYPE_SATA           2
#define STORAGE_TYPE_SSD            3
#define STORAGE_TYPE_HDD            4
#define STORAGE_TYPE_EXTERNAL       5
#define STORAGE_TYPE_RAID           6
#define STORAGE_TYPE_VIRTUAL        7

/*===========================================================================*/
/*                         FILESYSTEM TYPES                                  */
/*===========================================================================*/

#define FS_TYPE_UNKNOWN             0
#define FS_TYPE_NEXFS               1
#define FS_TYPE_EXT4                2
#define FS_TYPE_EXT3                3
#define FS_TYPE_EXT2                4
#define FS_TYPE_BTRFS               5
#define FS_TYPE_XFS                 6
#define FS_TYPE_FAT32               7
#define FS_TYPE_NTFS                8
#define FS_TYPE_EXFAT               9
#define FS_TYPE_ZFS                 10

/*===========================================================================*/
/*                         PARTITION TYPES                                   */
/*===========================================================================*/

#define PARTITION_TYPE_PRIMARY      0
#define PARTITION_TYPE_EXTENDED     1
#define PARTITION_TYPE_LOGICAL      2
#define PARTITION_TYPE_GPT          3

/*===========================================================================*/
/*                         HEALTH STATUS                                     */
/*===========================================================================*/

#define HEALTH_STATUS_UNKNOWN       0
#define HEALTH_STATUS_GOOD          1
#define HEALTH_STATUS_FAIR          2
#define HEALTH_STATUS_POOR          3
#define HEALTH_STATUS_CRITICAL      4
#define HEALTH_STATUS_FAILED        5

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * SMART Attribute
 */
typedef struct {
    u8 attr_id;                   /* Attribute ID */
    char name[64];                /* Attribute Name */
    u16 current_value;            /* Current Value */
    u16 worst_value;              /* Worst Value */
    u16 threshold;                /* Threshold */
    u64 raw_value;                /* Raw Value */
    u8 status;                    /* Status (OK/Warning/Bad) */
    char description[256];        /* Description */
} smart_attribute_t;

/**
 * SMART Data
 */
typedef struct {
    bool supported;               /* SMART Supported */
    bool enabled;                 /* SMART Enabled */
    bool healthy;                 /* Overall Health */
    u8 health_status;             /* Health Status */
    u16 temperature;              /* Temperature (Celsius) */
    u16 temperature_max;          /* Max Temperature */
    u32 power_on_hours;           /* Power On Hours */
    u32 power_cycle_count;        /* Power Cycle Count */
    u64 host_reads;               /* Host Read Commands */
    u64 host_writes;              /* Host Write Commands */
    u64 data_units_read;          /* Data Units Read (512B) */
    u64 data_units_written;       /* Data Units Written (512B) */
    u32 available_spare;          /* Available Spare (%) */
    u32 percentage_used;          /* Percentage Used */
    u32 media_errors;             /* Media Errors */
    u32 error_count;              /* Error Count */
    u64 warning_temp_time;        /* Warning Temperature Time */
    u64 critical_comp_time;       /* Critical Composite Time */
    smart_attribute_t attrs[STORAGE_MAX_SMART_ATTRS]; /* Attributes */
    u32 attr_count;               /* Attribute Count */
} smart_data_t;

/**
 * I/O Statistics
 */
typedef struct {
    u64 reads;                    /* Read Operations */
    u64 writes;                   /* Write Operations */
    u64 read_bytes;               /* Bytes Read */
    u64 write_bytes;              /* Bytes Written */
    u64 read_latency_avg;         /* Average Read Latency (ns) */
    u64 write_latency_avg;        /* Average Write Latency (ns) */
    u64 read_latency_max;         /* Maximum Read Latency */
    u64 write_latency_max;        /* Maximum Write Latency */
    u32 iops_read;                /* Read IOPS */
    u32 iops_write;               /* Write IOPS */
    u32 queue_depth;              /* Current Queue Depth */
    u32 utilization;              /* Utilization (%) */
    u64 timestamp;                /* Last Update Timestamp */
} io_stats_t;

/**
 * I/O History Sample
 */
typedef struct {
    u64 timestamp;                /* Timestamp */
    u32 read_iops;                /* Read IOPS */
    u32 write_iops;               /* Write IOPS */
    u32 read_throughput;          /* Read Throughput (KB/s) */
    u32 write_throughput;         /* Write Throughput (KB/s) */
    u32 temperature;              /* Temperature */
    u32 utilization;              /* Utilization */
} io_sample_t;

/**
 * Partition Information
 */
typedef struct {
    u32 partition_id;             /* Partition ID */
    u32 device_id;                /* Device ID */
    char name[64];                /* Partition Name */
    char device_path[128];        /* Device Path */
    char mount_point[256];        /* Mount Point */
    char label[64];               /* Partition Label */
    char uuid[64];                /* Partition UUID */
    u64 start_sector;             /* Start Sector */
    u64 end_sector;               /* End Sector */
    u64 size_bytes;               /* Size in Bytes */
    u64 used_bytes;               /* Used Space */
    u64 free_bytes;               /* Free Space */
    u32 fs_type;                  /* Filesystem Type */
    u32 partition_type;           /* Partition Type */
    bool is_mounted;              /* Is Mounted */
    bool is_boot;                 /* Is Boot Partition */
    bool is_root;                 /* Is Root Partition */
    bool is_encrypted;            /* Is Encrypted */
    bool is_readonly;             /* Is Read-Only */
    u32 usage_percent;            /* Usage Percentage */
} partition_info_t;

/**
 * Storage Device Information
 */
typedef struct {
    u32 device_id;                /* Device ID */
    u32 controller_id;            /* Controller ID */
    char name[64];                /* Device Name */
    char model[64];               /* Model Number */
    char serial[32];              /* Serial Number */
    char firmware[16];            /* Firmware Version */
    char device_path[128];        /* Device Path */
    char vendor[64];              /* Vendor Name */
    u32 device_type;              /* Device Type */
    u64 total_capacity;           /* Total Capacity */
    u64 used_capacity;            /* Used Capacity */
    u64 free_capacity;            /* Free Capacity */
    u32 block_size;               /* Block Size */
    u64 total_blocks;             /* Total Blocks */
    u32 rotation_rate;            /* Rotation Rate (RPM, 0 for SSD) */
    u32 form_factor;              /* Form Factor */
    u32 interface_type;           /* Interface Type */
    u32 interface_speed;          /* Interface Speed */
    
    /* Health */
    u32 health_status;            /* Health Status */
    u8 health_percent;            /* Health Percentage */
    smart_data_t smart;           /* SMART Data */
    
    /* Statistics */
    io_stats_t stats;             /* I/O Statistics */
    io_sample_t history[STORAGE_HISTORY_SAMPLES]; /* History */
    u32 history_index;            /* History Index */
    
    /* Partitions */
    partition_info_t *partitions; /* Partitions */
    u32 partition_count;          /* Partition Count */
    
    /* State */
    bool is_present;              /* Is Present */
    bool is_active;               /* Is Active */
    bool is_external;             /* Is External */
    bool supports_trim;           /* Supports TRIM */
    bool supports_smart;          /* Supports SMART */
    bool is_encrypted;            /* Is Encrypted */
    bool has_password;            /* Has Password */
    
    /* Temperature */
    u32 temperature;              /* Current Temperature */
    u32 temperature_min;          /* Minimum Temperature */
    u32 temperature_max;          /* Maximum Temperature */
    
    /* Power */
    u32 power_state;              /* Power State */
    u64 power_on_hours;           /* Power On Hours */
    u32 power_cycle_count;        /* Power Cycle Count */
    
    /* Error Status */
    u32 error_count;              /* Error Count */
    u32 media_errors;             /* Media Errors */
    char last_error[256];         /* Last Error Message */
    
    /* Driver-specific data */
    void *nvme_data;              /* NVMe Device Data */
    void *ahci_data;              /* AHCI Port Data */
} storage_device_t;

/**
 * Disk Operation
 */
typedef struct {
    u32 operation_id;             /* Operation ID */
    char name[64];                /* Operation Name */
    char description[256];        /* Operation Description */
    u32 operation_type;           /* Operation Type */
    u32 status;                   /* Operation Status */
    u32 progress;                 /* Progress (%) */
    u64 start_time;               /* Start Time */
    u64 estimated_remaining;      /* Estimated Remaining */
    char current_action[128];     /* Current Action */
    bool can_cancel;              /* Can Cancel */
    bool can_pause;               /* Can Pause */
    bool is_paused;               /* Is Paused */
} disk_operation_t;

/**
 * Storage Manager State
 */
typedef struct {
    bool initialized;             /* Is Initialized */
    bool monitoring_active;       /* Is Monitoring Active */
    
    /* Window references */
    window_t *main_window;
    widget_t *main_widget;
    widget_t *sidebar;
    widget_t *content_area;
    widget_t *device_list;
    widget_t *device_info_panel;
    widget_t *smart_panel;
    widget_t *stats_panel;
    widget_t *partition_panel;
    widget_t *toolbar;
    widget_t *status_bar;
    
    /* Devices */
    storage_device_t devices[STORAGE_MAX_DEVICES]; /* Devices */
    u32 device_count;             /* Device Count */
    u32 selected_device;          /* Selected Device */
    
    /* Partitions */
    partition_info_t partitions[STORAGE_MAX_PARTITIONS]; /* Partitions */
    u32 partition_count;          /* Partition Count */
    
    /* Operations */
    disk_operation_t operations[16]; /* Active Operations */
    u32 operation_count;          /* Operation Count */
    
    /* Monitoring */
    u64 last_update;              /* Last Update Time */
    u32 update_interval;          /* Update Interval (ms) */
    bool auto_refresh;            /* Auto Refresh */
    
    /* Display options */
    u32 view_mode;                /* View Mode */
    bool show_smart;              /* Show SMART */
    bool show_stats;              /* Show Statistics */
    bool show_partitions;         /* Show Partitions */
    bool show_graphs;             /* Show Graphs */
    u32 graph_type;               /* Graph Type */
    
    /* Alerts */
    bool alerts_enabled;          /* Alerts Enabled */
    u32 temp_warning_threshold;   /* Temperature Warning */
    u32 temp_critical_threshold;  /* Temperature Critical */
    u32 health_warning_threshold; /* Health Warning */
    u32 space_warning_threshold;  /* Space Warning */
    
    /* Callbacks */
    void (*on_device_selected)(struct storage_manager *, u32);
    void (*on_partition_mounted)(struct storage_manager *, u32);
    void (*on_partition_unmounted)(struct storage_manager *, u32);
    void (*on_operation_started)(struct storage_manager *, u32);
    void (*on_operation_completed)(struct storage_manager *, u32);
    void (*on_alert)(struct storage_manager *, u32, const char *);
    
    /* User data */
    void *user_data;
    
} storage_manager_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Manager lifecycle */
int storage_manager_init(storage_manager_t *manager);
int storage_manager_run(storage_manager_t *manager);
int storage_manager_shutdown(storage_manager_t *manager);
int storage_manager_refresh(storage_manager_t *manager);

/* Device enumeration */
int storage_manager_enumerate(storage_manager_t *manager);
int storage_manager_add_device(storage_manager_t *manager, storage_device_t *device);
int storage_manager_remove_device(storage_manager_t *manager, u32 device_id);
storage_device_t *storage_manager_get_device(storage_manager_t *manager, u32 device_id);
int storage_manager_select_device(storage_manager_t *manager, u32 device_id);

/* Device information */
int storage_manager_get_device_info(storage_manager_t *manager, u32 device_id, storage_device_t *info);
int storage_manager_update_device_info(storage_manager_t *manager, u32 device_id);
int storage_manager_get_all_devices(storage_manager_t *manager, storage_device_t *devices, u32 *count);

/* SMART operations */
int storage_manager_get_smart(storage_manager_t *manager, u32 device_id, smart_data_t *smart);
int storage_manager_update_smart(storage_manager_t *manager, u32 device_id);
bool storage_manager_is_healthy(storage_manager_t *manager, u32 device_id);
int storage_manager_get_temperature(storage_manager_t *manager, u32 device_id);

/* Statistics */
int storage_manager_get_stats(storage_manager_t *manager, u32 device_id, io_stats_t *stats);
int storage_manager_update_stats(storage_manager_t *manager, u32 device_id);
int storage_manager_get_history(storage_manager_t *manager, u32 device_id, io_sample_t *samples, u32 *count);
int storage_manager_reset_stats(storage_manager_t *manager, u32 device_id);

/* Partition operations */
int storage_manager_list_partitions(storage_manager_t *manager, partition_info_t *partitions, u32 *count);
int storage_manager_mount_partition(storage_manager_t *manager, u32 partition_id, const char *mount_point);
int storage_manager_unmount_partition(storage_manager_t *manager, u32 partition_id);
int storage_manager_format_partition(storage_manager_t *manager, u32 partition_id, u32 fs_type);
int storage_manager_create_partition(storage_manager_t *manager, u32 device_id, u64 start, u64 size, u32 type);
int storage_manager_delete_partition(storage_manager_t *manager, u32 partition_id);
int storage_manager_resize_partition(storage_manager_t *manager, u32 partition_id, u64 new_size);

/* Disk operations */
int storage_manager_secure_erase(storage_manager_t *manager, u32 device_id);
int storage_manager_trim_device(storage_manager_t *manager, u32 device_id);
int storage_manager_check_health(storage_manager_t *manager, u32 device_id);
int storage_manager_run_self_test(storage_manager_t *manager, u32 device_id, u32 test_type);

/* Encryption */
int storage_manager_encrypt_device(storage_manager_t *manager, u32 device_id, const char *password);
int storage_manager_decrypt_device(storage_manager_t *manager, u32 device_id, const char *password);
int storage_manager_lock_device(storage_manager_t *manager, u32 device_id);
int storage_manager_unlock_device(storage_manager_t *manager, u32 device_id, const char *password);

/* UI updates */
int storage_manager_update_ui(storage_manager_t *manager);
int storage_manager_update_device_list(storage_manager_t *manager);
int storage_manager_update_smart_display(storage_manager_t *manager);
int storage_manager_update_stats_display(storage_manager_t *manager);
int storage_manager_update_graphs(storage_manager_t *manager);

/* Alerts */
int storage_manager_check_alerts(storage_manager_t *manager);
int storage_manager_add_alert(storage_manager_t *manager, u32 device_id, const char *message);
int storage_manager_clear_alerts(storage_manager_t *manager);

/* Utility functions */
const char *storage_get_device_type_name(u32 type);
const char *storage_get_fs_type_name(u32 type);
const char *storage_get_health_status_name(u32 status);
const char *storage_format_size(u64 size, char *buffer, u32 buffer_size);
u32 storage_calculate_usage_percent(u64 used, u64 total);

/* Global instance */
storage_manager_t *storage_manager_get_instance(void);

#endif /* _STORAGE_MANAGER_H */
