/*
 * NEXUS OS - Disk Monitor
 * apps/disk-monitor/disk-monitor.h
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * Comprehensive disk monitoring:
 * - Disk usage
 * - I/O monitoring
 * - SMART status
 * - Temperature
 * - Health status
 * - Partition management
 * - Filesystem monitoring
 */

#ifndef _NEXUS_DISK_MONITOR_H
#define _NEXUS_DISK_MONITOR_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         DISK MONITORING STRUCTURES                        */
/*===========================================================================*/

/**
 * disk_info_t - Disk information
 */
typedef struct {
    char device[64];
    char model[256];
    char serial[64];
    char type[32];  /* HDD, SSD, NVMe */
    
    /* Capacity */
    u64 total_bytes;
    u64 used_bytes;
    u64 free_bytes;
    float usage_percent;
    
    /* Geometry */
    u32 sector_size;
    u64 sector_count;
    u32 cylinders;
    u32 heads;
    
    /* Health */
    bool smart_supported;
    bool smart_enabled;
    bool healthy;
    u32 power_on_hours;
    u32 power_cycle_count;
    u32 temperature_celsius;
    u32 wear_level;
    
    /* Performance */
    u64 read_speed;
    u64 write_speed;
    u64 iops_read;
    u64 iops_write;
    float latency_ms;
    
    /* Errors */
    u32 read_errors;
    u32 write_errors;
    u32 reallocated_sectors;
    u32 pending_sectors;
    u32 uncorrectable_errors;
    
    /* Status */
    bool spinning;
    bool sleeping;
    bool encrypted;
    bool removable;
} disk_info_t;

/**
 * partition_info_t - Partition information
 */
typedef struct {
    char device[64];
    char mount_point[256];
    char filesystem[32];
    char label[128];
    char uuid[64];
    
    /* Size */
    u64 total_bytes;
    u64 used_bytes;
    u64 free_bytes;
    float usage_percent;
    
    /* Options */
    bool readonly;
    bool noexec;
    bool nosuid;
    bool nodev;
    
    /* Usage */
    u32 file_count;
    u32 directory_count;
    u32 symlink_count;
    
    /* Inodes */
    u64 total_inodes;
    u64 used_inodes;
    u64 free_inodes;
    float inode_usage_percent;
} partition_info_t;

/**
 * io_stats_t - I/O statistics
 */
typedef struct {
    char device[64];
    
    /* Reads */
    u64 reads;
    u64 sectors_read;
    u64 bytes_read;
    u64 read_time_ms;
    
    /* Writes */
    u64 writes;
    u64 sectors_written;
    u64 bytes_written;
    u64 write_time_ms;
    
    /* Queue */
    u32 queue_depth;
    u64 time_in_queue_ms;
    
    /* Rates */
    u64 read_rate_bps;
    u64 write_rate_bps;
    u64 io_rate_ops;
    
    /* Utilization */
    float utilization_percent;
    bool busy;
} io_stats_t;

/**
 * smart_attr_t - SMART attribute
 */
typedef struct {
    u8 id;
    char name[64];
    u32 value;
    u32 worst;
    u32 threshold;
    u64 raw;
    bool warning;
    bool failed;
    char description[256];
} smart_attr_t;

/**
 * disk_alert_t - Disk alert
 */
typedef struct {
    char device[64];
    char message[256];
    int severity;  /* 0=info, 1=warning, 2=critical, 3=failure */
    u64 timestamp;
    char recommendation[512];
} disk_alert_t;

/*===========================================================================*/
/*                         DISK MONITOR API                                  */
/*===========================================================================*/

/* Initialization */
int disk_monitor_init(void);
int disk_monitor_shutdown(void);

/* Disk enumeration */
int disk_monitor_scan(void);
disk_info_t *disk_monitor_get_disk(const char *device);
disk_info_t *disk_monitor_get_all_disks(int *count);
int disk_monitor_get_disk_count(void);

/* Partition monitoring */
partition_info_t *partition_monitor_get_all(int *count);
partition_info_t *partition_monitor_get_by_mount(const char *mount);
int partition_monitor_refresh(void);

/* I/O monitoring */
io_stats_t *io_monitor_get_stats(const char *device);
int io_monitor_get_all_stats(io_stats_t **stats, int *count);
int io_monitor_reset_stats(const char *device);
int io_monitor_start_realtime(void);
int io_monitor_stop_realtime(void);

/* SMART monitoring */
int smart_monitor_enable(const char *device);
int smart_monitor_disable(const char *device);
int smart_monitor_test(const char *device, int test_type);
smart_attr_t *smart_monitor_get_attributes(const char *device, int *count);
int smart_monitor_get_health(const char *device);
const char *smart_monitor_get_status_string(int status);

/* Temperature monitoring */
int disk_monitor_get_temperature(const char *device);
int disk_monitor_get_temperature_history(const char *device, u32 **temps, int *count);

/* Alert system */
int disk_alert_set_threshold(const char *device, const char *metric, float threshold);
int disk_alert_get_active(disk_alert_t **alerts, int *count);
int disk_alert_clear(const char *device);
int disk_alert_clear_all(void);

/* Filesystem monitoring */
int fs_monitor_get_usage(const char *mount_point, u64 *total, u64 *used, u64 *free);
int fs_monitor_get_file_count(const char *path);
int fs_monitor_get_directory_size(const char *path);
int fs_monitor_find_large_files(const char *path, u64 min_size, char ***files, int *count);

/* Partition management */
int partition_create(const char *device, u64 start, u64 size, const char *type);
int partition_delete(const char *partition);
int partition_resize(const char *partition, u64 new_size);
int partition_format(const char *partition, const char *filesystem);
int partition_mount(const char *partition, const char *mount_point, const char *options);
int partition_unmount(const char *mount_point);

/* Disk operations */
int disk_check_health(const char *device);
int disk_check_errors(const char *device);
int disk_trim_ssd(const char *device);
int disk_defrag(const char *device);
int disk_wipe(const char *device, int passes);
int disk_clone(const char *source, const char *dest);

/* Benchmarking */
int disk_benchmark_read(const char *device, u64 *speed);
int disk_benchmark_write(const char *device, u64 *speed);
int disk_benchmark_random_read(const char *device, u64 *iops);
int disk_benchmark_random_write(const char *device, u64 *iops);

/* History and logging */
int disk_monitor_get_history(const char *device, const char *metric, u64 **values, int *count);
int disk_monitor_clear_history(void);
int disk_monitor_export_report(const char *path);

/* GUI integration */
int disk_monitor_gui_init(void);
int disk_monitor_gui_show(void);
int disk_monitor_gui_show_disk(const char *device);

#endif /* _NEXUS_DISK_MONITOR_H */
