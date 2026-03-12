/*
 * NEXUS OS - System Monitor
 * apps/system-monitor/system-monitor.h
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * Real-time system monitoring:
 * - CPU usage
 * - Memory usage
 * - Disk I/O
 * - Network I/O
 * - Process list
 * - Temperature
 * - Power
 */

#ifndef _NEXUS_SYSTEM_MONITOR_H
#define _NEXUS_SYSTEM_MONITOR_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         MONITORING STRUCTURES                             */
/*===========================================================================*/

/**
 * cpu_stats_t - CPU statistics
 */
typedef struct {
    u32 cpu_count;
    u64 total_time;
    u64 user_time;
    u64 system_time;
    u64 idle_time;
    u64 iowait_time;
    u64 irq_time;
    u64 softirq_time;
    float usage_percent;
    float user_percent;
    float system_percent;
    float idle_percent;
    u64 frequency_mhz;
    u64 temperature;
    char governor[32];
} cpu_stats_t;

/**
 * memory_stats_t - Memory statistics
 */
typedef struct {
    u64 total;
    u64 used;
    u64 free;
    u64 shared;
    u64 buffers;
    u64 cached;
    u64 available;
    float usage_percent;
    
    /* Swap */
    u64 swap_total;
    u64 swap_used;
    u64 swap_free;
    float swap_percent;
} memory_stats_t;

/**
 * disk_stats_t - Disk statistics
 */
typedef struct {
    char device[64];
    char mount_point[256];
    char filesystem[32];
    
    u64 total;
    u64 used;
    u64 free;
    float usage_percent;
    
    /* I/O */
    u64 read_bytes;
    u64 write_bytes;
    u64 read_ops;
    u64 write_ops;
    u64 read_speed;
    u64 write_speed;
} disk_stats_t;

/**
 * network_stats_t - Network statistics
 */
typedef struct {
    char interface[64];
    char ip_address[64];
    char mac_address[32];
    
    /* Traffic */
    u64 rx_bytes;
    u64 tx_bytes;
    u64 rx_packets;
    u64 tx_packets;
    u64 rx_errors;
    u64 tx_errors;
    
    /* Speed */
    u64 rx_speed;
    u64 tx_speed;
    u64 link_speed;
    
    bool connected;
} network_stats_t;

/**
 * process_info_t - Process information
 */
typedef struct process_info {
    pid_t pid;
    pid_t ppid;
    char name[256];
    char cmdline[1024];
    char user[64];
    
    /* State */
    char state;
    u64 start_time;
    u64 cpu_time;
    float cpu_percent;
    
    /* Memory */
    u64 memory;
    float memory_percent;
    u64 virtual_memory;
    
    /* I/O */
    u64 read_bytes;
    u64 write_bytes;
    
    /* Threads */
    int thread_count;
    
    /* Links */
    struct process_info *next;
} process_info_t;

/**
 * system_stats_t - Complete system statistics
 */
typedef struct {
    /* Timestamp */
    u64 timestamp;
    
    /* CPU */
    cpu_stats_t cpu;
    cpu_stats_t *cpu_cores;
    
    /* Memory */
    memory_stats_t memory;
    
    /* Disk */
    disk_stats_t *disks;
    int disk_count;
    
    /* Network */
    network_stats_t *networks;
    int network_count;
    
    /* Processes */
    process_info_t *processes;
    int process_count;
    
    /* System */
    u64 uptime;
    char hostname[64];
    char kernel_version[64];
    float load_average[3];
    
    /* Power */
    int battery_percent;
    bool on_battery;
    float power_watts;
} system_stats_t;

/*===========================================================================*/
/*                         SYSTEM MONITOR API                                */
/*===========================================================================*/

/* Initialization */
int system_monitor_init(void);
int system_monitor_shutdown(void);

/* Statistics collection */
int system_monitor_update(system_stats_t *stats);
int system_monitor_get_cpu_stats(cpu_stats_t *stats);
int system_monitor_get_memory_stats(memory_stats_t *stats);
int system_monitor_get_disk_stats(disk_stats_t *stats, const char *device);
int system_monitor_get_network_stats(network_stats_t *stats, const char *interface);
int system_monitor_get_process_list(process_info_t **procs, int *count);

/* Process operations */
int system_monitor_get_process(pid_t pid, process_info_t *info);
int system_monitor_kill_process(pid_t pid, int signal);
int system_monitor_set_priority(pid_t pid, int priority);

/* Real-time monitoring */
int system_monitor_start_realtime(void);
int system_monitor_stop_realtime(void);
int system_monitor_set_interval(int milliseconds);

/* Alerts */
int system_monitor_set_alert(const char *metric, float threshold);
int system_monitor_clear_alert(const char *metric);
int system_monitor_get_alerts(void);

/* History */
int system_monitor_get_history(const char *metric, u64 **values, int *count);
int system_monitor_clear_history(void);

/* Export */
int system_monitor_export_csv(const char *path);
int system_monitor_export_json(const char *path);

#endif /* _NEXUS_SYSTEM_MONITOR_H */
