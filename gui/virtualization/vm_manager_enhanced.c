/*
 * NEXUS OS - VM Manager Enhanced GUI
 * gui/virtualization/vm_manager_enhanced.c
 *
 * Enterprise VM management with detailed stats, extensions, and modes
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         VM STATISTICS DISPLAY                             */
/*===========================================================================*/

/* VM Detailed Statistics */
typedef struct {
    /* Basic Info */
    char name[128];
    char uuid[64];
    char os_type[32];
    char os_version[32];
    u32 vm_id;
    u32 state;  /* Running, Paused, Stopped */
    
    /* CPU Statistics */
    u32 cpu_count;
    u32 cpu_cores;
    u32 cpu_threads;
    u64 cpu_usage_percent;
    u64 cpu_time_total;
    u64 cpu_time_user;
    u64 cpu_time_system;
    u64 cpu_time_idle;
    u64 cpu_time_wait;
    u32 cpu_speed_mhz;
    char cpu_model[64];
    
    /* Memory Statistics */
    u64 memory_total_mb;
    u64 memory_used_mb;
    u64 memory_free_mb;
    u64 memory_cached_mb;
    u64 memory_buffered_mb;
    u32 memory_usage_percent;
    u64 memory_swap_total_mb;
    u64 memory_swap_used_mb;
    u64 memory_swap_free_mb;
    u32 memory_ballooned_mb;
    u32 memory_shared_mb;
    
    /* Storage Statistics */
    u32 disk_count;
    struct {
        char name[64];
        char path[256];
        u64 total_gb;
        u64 used_gb;
        u64 free_gb;
        u32 usage_percent;
        u64 read_bytes;
        u64 write_bytes;
        u64 read_iops;
        u64 write_iops;
        char type[16];  /* SSD, HDD, NVMe */
    } disks[8];
    
    /* Network Statistics */
    u32 network_count;
    struct {
        char name[32];
        char mac[32];
        char ip[32];
        char ipv6[64];
        u64 rx_bytes;
        u64 tx_bytes;
        u64 rx_packets;
        u64 tx_packets;
        u64 rx_errors;
        u64 tx_errors;
        u64 rx_dropped;
        u64 tx_dropped;
        u32 speed_mbps;
        char status[16];  /* Connected, Disconnected */
    } networks[8];
    
    /* GPU Statistics */
    u32 gpu_count;
    struct {
        char name[64];
        char vendor[32];
        u64 memory_total_mb;
        u64 memory_used_mb;
        u32 usage_percent;
        u32 temperature_c;
        u32 fan_speed_percent;
    } gpus[4];
    
    /* Process Statistics */
    u32 process_count;
    u32 thread_count;
    u32 handle_count;
    
    /* Uptime */
    u64 uptime_seconds;
    u64 boot_time;
    
    /* Performance Metrics */
    u32 cpu_ready_percent;
    u32 cpu_stolen_percent;
    u32 memory_pressure;
    u32 disk_latency_ms;
    u32 network_latency_ms;
    
    /* Snapshots */
    u32 snapshot_count;
    u64 snapshot_total_size_gb;
    
    /* Extensions */
    u32 extension_count;
    char extensions[16][64];
} vm_detailed_stats_t;

/* VM Mode */
#define VM_MODE_NORMAL          0
#define VM_MODE_IDLE            1
#define VM_MODE_POWER_SAVE      2
#define VM_MODE_PERFORMANCE     3
#define VM_MODE_HIGH_PRIORITY   4
#define VM_MODE_BACKGROUND      5
#define VM_MODE_SUSPENDED       6

/* VM Extension */
typedef struct {
    char name[64];
    char version[32];
    char description[256];
    bool enabled;
    bool auto_start;
    char author[64];
    char dependencies[8][64];
    u32 dependency_count;
    u64 install_date;
    u64 last_update;
    u32 size_kb;
} vm_extension_t;

/* Enhanced VM Card for Display */
typedef struct {
    u32 vm_id;
    char name[128];
    char status[16];
    u32 status_color;
    
    /* Quick Stats */
    u32 cpu_count;
    u32 cpu_usage_percent;
    u64 memory_total_mb;
    u64 memory_used_mb;
    u32 memory_usage_percent;
    u64 disk_total_gb;
    u64 disk_used_gb;
    u32 disk_usage_percent;
    
    /* Network */
    char primary_ip[32];
    u64 network_rx_mbps;
    u64 network_tx_mbps;
    
    /* Mode */
    u32 mode;
    char mode_name[32];
    
    /* Extensions */
    u32 extension_count;
    bool extensions_enabled;
    
    /* Uptime */
    u64 uptime_seconds;
    
    /* Health */
    u32 health_status;  /* 0=Good, 1=Warning, 2=Critical */
    char health_message[64];
    
    bool selected;
    bool favorite;
    char icon[8];
} vm_enhanced_card_t;

/* Global VM Manager Enhanced */
typedef struct {
    vm_detailed_stats_t *stats;
    vm_enhanced_card_t cards[64];
    u32 card_count;
    vm_extension_t extensions[32];
    u32 extension_count;
    u32 display_mode;  /* 0=List, 1=Grid, 2=Details */
    u32 sort_by;  /* 0=Name, 1=CPU, 2=Memory, 3=Storage */
    bool show_all_stats;
    bool auto_refresh;
    u32 refresh_interval_seconds;
} vm_manager_enhanced_t;

static vm_manager_enhanced_t g_vm_enhanced;

/*===========================================================================*/
/*                         VM STATISTICS FUNCTIONS                           */
/*===========================================================================*/

/* Get CPU Count Display String */
void vm_format_cpu_count(u32 cores, u32 threads, char *output, u32 size)
{
    snprintf(output, size, "%d Cores, %d Threads", cores, threads);
}

/* Format Memory Size */
void vm_format_memory(u64 mb, char *output, u32 size)
{
    if (mb >= 1024) {
        snprintf(output, size, "%.1f GB", (double)mb / 1024.0);
    } else {
        snprintf(output, size, "%lu MB", mb);
    }
}

/* Format Storage Size */
void vm_format_storage(u64 gb, char *output, u32 size)
{
    if (gb >= 1024) {
        snprintf(output, size, "%.1f TB", (double)gb / 1024.0);
    } else {
        snprintf(output, size, "%lu GB", gb);
    }
}

/* Format Uptime */
void vm_format_uptime(u64 seconds, char *output, u32 size)
{
    u64 days = seconds / 86400;
    u64 hours = (seconds % 86400) / 3600;
    u64 minutes = (seconds % 3600) / 60;
    
    if (days > 0) {
        snprintf(output, size, "%lu days, %lu hours, %lu min", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(output, size, "%lu hours, %lu min", hours, minutes);
    } else {
        snprintf(output, size, "%lu min", minutes);
    }
}

/* Get Status Color */
u32 vm_get_status_color(const char *status)
{
    if (strcmp(status, "Running") == 0) {
        return 0x27AE60;  /* Green */
    } else if (strcmp(status, "Paused") == 0) {
        return 0xF39C12;  /* Orange */
    } else if (strcmp(status, "Stopped") == 0) {
        return 0x7F8C8D;  /* Gray */
    } else if (strcmp(status, "Error") == 0) {
        return 0xE74C3C;  /* Red */
    }
    return 0x3498DB;  /* Blue */
}

/* Get Mode Name */
void vm_get_mode_name(u32 mode, char *output, u32 size)
{
    const char *modes[] = {
        "Normal",
        "Idle",
        "Power Save",
        "Performance",
        "High Priority",
        "Background",
        "Suspended"
    };
    
    if (mode < 7) {
        strncpy(output, modes[mode], size - 1);
    } else {
        strncpy(output, "Unknown", size - 1);
    }
}

/* Get Health Status */
u32 vm_calculate_health(vm_detailed_stats_t *stats)
{
    if (!stats) return 0;
    
    /* Check CPU */
    if (stats->cpu_usage_percent > 95) {
        return 2;  /* Critical */
    }
    
    /* Check Memory */
    if (stats->memory_usage_percent > 95) {
        return 2;  /* Critical */
    }
    
    /* Check Storage */
    for (u32 i = 0; i < stats->disk_count; i++) {
        if (stats->disks[i].usage_percent > 95) {
            return 2;  /* Critical */
        }
    }
    
    /* Check for warnings */
    if (stats->cpu_usage_percent > 80 ||
        stats->memory_usage_percent > 80) {
        return 1;  /* Warning */
    }
    
    for (u32 i = 0; i < stats->disk_count; i++) {
        if (stats->disks[i].usage_percent > 80) {
            return 1;  /* Warning */
        }
    }
    
    return 0;  /* Good */
}

/*===========================================================================*/
/*                         VM EXTENSIONS                                     */
/*===========================================================================*/

/* Install VM Extension */
int vm_extension_install(const char *name, const char *version, const char *description)
{
    if (g_vm_enhanced.extension_count >= 32) {
        printk("[VM-EXT] ERROR: Maximum extensions reached\n");
        return -1;
    }
    
    vm_extension_t *ext = &g_vm_enhanced.extensions[g_vm_enhanced.extension_count++];
    memset(ext, 0, sizeof(vm_extension_t));
    
    strncpy(ext->name, name, 63);
    strncpy(ext->version, version, 31);
    strncpy(ext->description, description, 255);
    ext->enabled = true;
    ext->auto_start = true;
    strncpy(ext->author, "NEXUS OS", 63);
    ext->size_kb = 0;
    
    printk("[VM-EXT] Installed extension: %s v%s\n", name, version);
    printk("[VM-EXT]   Description: %s\n", description);
    
    return 0;
}

/* Enable/Disable Extension */
int vm_extension_toggle(const char *name, bool enabled)
{
    for (u32 i = 0; i < g_vm_enhanced.extension_count; i++) {
        vm_extension_t *ext = &g_vm_enhanced.extensions[i];
        if (strcmp(ext->name, name) == 0) {
            ext->enabled = enabled;
            printk("[VM-EXT] Extension '%s' %s\n", name, enabled ? "enabled" : "disabled");
            return 0;
        }
    }
    return -1;
}

/* List Extensions */
void vm_extension_list(void)
{
    printk("\n[VM-EXT] ====== EXTENSIONS ======\n");
    printk("[VM-EXT] Total: %u / 32\n", g_vm_enhanced.extension_count);
    
    for (u32 i = 0; i < g_vm_enhanced.extension_count; i++) {
        vm_extension_t *ext = &g_vm_enhanced.extensions[i];
        printk("[VM-EXT] %u. %s v%s\n", i + 1, ext->name, ext->version);
        printk("[VM-EXT]    %s\n", ext->description);
        printk("[VM-EXT]    Status: %s\n", ext->enabled ? "Enabled" : "Disabled");
        printk("[VM-EXT]    Auto-start: %s\n", ext->auto_start ? "Yes" : "No");
    }
    
    printk("[VM-EXT] ==========================\n\n");
}

/* Initialize Default Extensions */
void vm_extension_init_defaults(void)
{
    vm_extension_install("VMware Tools", "12.0.0", "Enhanced VMware integration and drivers");
    vm_extension_install("VirtualBox GA", "7.0.0", "VirtualBox Guest Additions support");
    vm_extension_install("QEMU Guest Agent", "8.0.0", "QEMU guest agent for management");
    vm_extension_install("Shared Folders", "1.0.0", "Host-guest folder sharing");
    vm_extension_install("Drag & Drop", "1.0.0", "Host-guest drag and drop support");
    vm_extension_install("Clipboard Share", "1.0.0", "Shared clipboard between host and guest");
    vm_extension_install("Time Sync", "1.0.0", "Automatic time synchronization");
    vm_extension_install("Auto Resize", "1.0.0", "Automatic display resolution adjustment");
}

/*===========================================================================*/
/*                         VM MODES                                          */
/*===========================================================================*/

/* Set VM Mode */
int vm_set_mode(u32 vm_id, u32 mode)
{
    printk("[VM-MODE] Setting VM %u to mode %u\n", vm_id, mode);
    
    for (u32 i = 0; i < g_vm_enhanced.card_count; i++) {
        vm_enhanced_card_t *card = &g_vm_enhanced.cards[i];
        if (card->vm_id == vm_id) {
            card->mode = mode;
            vm_get_mode_name(mode, card->mode_name, 32);
            
            printk("[VM-MODE] VM '%s' mode: %s\n", card->name, card->mode_name);
            
            /* Apply mode settings */
            switch (mode) {
            case VM_MODE_IDLE:
                printk("[VM-MODE]   CPU throttling enabled\n");
                printk("[VM-MODE]   Reduced priority\n");
                break;
            case VM_MODE_POWER_SAVE:
                printk("[VM-MODE]   Power saving mode\n");
                printk("[VM-MODE]   CPU frequency reduced\n");
                break;
            case VM_MODE_PERFORMANCE:
                printk("[VM-MODE]   Performance mode\n");
                printk("[VM-MODE]   CPU frequency max\n");
                printk("[VM-MODE]   I/O priority high\n");
                break;
            case VM_MODE_HIGH_PRIORITY:
                printk("[VM-MODE]   High priority mode\n");
                printk("[VM-MODE]   CPU affinity set\n");
                break;
            case VM_MODE_BACKGROUND:
                printk("[VM-MODE]   Background mode\n");
                printk("[VM-MODE]   Resource limits applied\n");
                break;
            }
            
            return 0;
        }
    }
    
    return -1;
}

/* Get Recommended Mode */
u32 vm_get_recommended_mode(vm_detailed_stats_t *stats)
{
    if (!stats) return VM_MODE_NORMAL;
    
    /* High CPU usage - recommend performance mode */
    if (stats->cpu_usage_percent > 80) {
        return VM_MODE_PERFORMANCE;
    }
    
    /* Low activity - recommend idle mode */
    if (stats->cpu_usage_percent < 5 && stats->memory_usage_percent < 20) {
        return VM_MODE_IDLE;
    }
    
    /* Background task - recommend background mode */
    if (stats->process_count > 100) {
        return VM_MODE_BACKGROUND;
    }
    
    return VM_MODE_NORMAL;
}

/*===========================================================================*/
/*                         DISPLAY FUNCTIONS                                 */
/*===========================================================================*/

/* Render VM Card with Enhanced Stats */
void vm_render_enhanced_card(vm_enhanced_card_t *card, u32 x, u32 y, u32 width, u32 height)
{
    printk("[VM-DISPLAY] Rendering card for '%s' at (%u,%u)\n", card->name, x, y);
    printk("[VM-DISPLAY]   Status: %s (color: 0x%x)\n", card->status, card->status_color);
    printk("[VM-DISPLAY]   CPU: %u cores, %u%% usage\n", card->cpu_count, card->cpu_usage_percent);
    printk("[VM-DISPLAY]   Memory: %lu MB / %lu MB (%u%%)\n", 
           card->memory_used_mb, card->memory_total_mb, card->memory_usage_percent);
    printk("[VM-DISPLAY]   Storage: %lu GB / %lu GB (%u%%)\n",
           card->disk_used_gb, card->disk_total_gb, card->disk_usage_percent);
    printk("[VM-DISPLAY]   Network: %lu Mbps RX, %lu Mbps TX\n",
           card->network_rx_mbps, card->network_tx_mbps);
    printk("[VM-DISPLAY]   Mode: %s\n", card->mode_name);
    printk("[VM-DISPLAY]   Extensions: %u (enabled: %s)\n",
           card->extension_count, card->extensions_enabled ? "Yes" : "No");
    printk("[VM-DISPLAY]   Uptime: %lu seconds\n", card->uptime_seconds);
    printk("[VM-DISPLAY]   Health: %u (%s)\n",
           card->health_status,
           card->health_status == 0 ? "Good" : 
           card->health_status == 1 ? "Warning" : "Critical");
}

/* Render Detailed VM Stats */
void vm_render_detailed_stats(vm_detailed_stats_t *stats)
{
    if (!stats) return;
    
    printk("\n[VM-STATS] ========================================\n");
    printk("[VM-STATS] VM: %s\n", stats->name);
    printk("[VM-STATS] ========================================\n");
    
    /* CPU */
    printk("[VM-STATS] CPU:\n");
    char cpu_str[64];
    vm_format_cpu_count(stats->cpu_cores, stats->cpu_threads, cpu_str, 64);
    printk("[VM-STATS]   Count: %s\n", cpu_str);
    printk("[VM-STATS]   Model: %s\n", stats->cpu_model);
    printk("[VM-STATS]   Speed: %u MHz\n", stats->cpu_speed_mhz);
    printk("[VM-STATS]   Usage: %lu%%\n", stats->cpu_usage_percent);
    printk("[VM-STATS]   Time: User=%lu, System=%lu, Idle=%lu, Wait=%lu\n",
           stats->cpu_time_user, stats->cpu_time_system,
           stats->cpu_time_idle, stats->cpu_time_wait);
    
    /* Memory */
    printk("[VM-STATS] Memory:\n");
    char mem_str[32];
    vm_format_memory(stats->memory_total_mb, mem_str, 32);
    printk("[VM-STATS]   Total: %s\n", mem_str);
    vm_format_memory(stats->memory_used_mb, mem_str, 32);
    printk("[VM-STATS]   Used: %s (%u%%)\n", mem_str, stats->memory_usage_percent);
    vm_format_memory(stats->memory_free_mb, mem_str, 32);
    printk("[VM-STATS]   Free: %s\n", mem_str);
    vm_format_memory(stats->memory_cached_mb, mem_str, 32);
    printk("[VM-STATS]   Cached: %s\n", mem_str);
    vm_format_memory(stats->memory_ballooned_mb, mem_str, 32);
    printk("[VM-STATS]   Ballooned: %s\n", mem_str);
    vm_format_memory(stats->memory_shared_mb, mem_str, 32);
    printk("[VM-STATS]   Shared: %s\n", mem_str);
    
    /* Storage */
    printk("[VM-STATS] Storage:\n");
    for (u32 i = 0; i < stats->disk_count; i++) {
        char size_str[32];
        vm_format_storage(stats->disks[i].total_gb, size_str, 32);
        printk("[VM-STATS]   Disk %u: %s (%s)\n", i + 1, size_str, stats->disks[i].type);
        printk("[VM-STATS]     Used: %lu GB (%u%%)\n",
               stats->disks[i].used_gb, stats->disks[i].usage_percent);
        printk("[VM-STATS]     I/O: Read=%lu, Write=%lu bytes\n",
               stats->disks[i].read_bytes, stats->disks[i].write_bytes);
        printk("[VM-STATS]     IOPS: Read=%lu, Write=%lu\n",
               stats->disks[i].read_iops, stats->disks[i].write_iops);
    }
    
    /* Network */
    printk("[VM-STATS] Network:\n");
    for (u32 i = 0; i < stats->network_count; i++) {
        printk("[VM-STATS]   NIC %u: %s\n", i + 1, stats->networks[i].name);
        printk("[VM-STATS]     MAC: %s\n", stats->networks[i].mac);
        printk("[VM-STATS]     IP: %s\n", stats->networks[i].ip);
        printk("[VM-STATS]     Status: %s\n", stats->networks[i].status);
        printk("[VM-STATS]     Speed: %u Mbps\n", stats->networks[i].speed_mbps);
        printk("[VM-STATS]     Traffic: RX=%lu, TX=%lu bytes\n",
               stats->networks[i].rx_bytes, stats->networks[i].tx_bytes);
        printk("[VM-STATS]     Packets: RX=%lu, TX=%lu\n",
               stats->networks[i].rx_packets, stats->networks[i].tx_packets);
    }
    
    /* GPU */
    if (stats->gpu_count > 0) {
        printk("[VM-STATS] GPU:\n");
        for (u32 i = 0; i < stats->gpu_count; i++) {
            printk("[VM-STATS]   GPU %u: %s (%s)\n",
                   i + 1, stats->gpus[i].name, stats->gpus[i].vendor);
            printk("[VM-STATS]     Memory: %lu MB / %lu MB\n",
                   stats->gpus[i].memory_used_mb, stats->gpus[i].memory_total_mb);
            printk("[VM-STATS]     Usage: %u%%\n", stats->gpus[i].usage_percent);
            printk("[VM-STATS]     Temp: %u°C\n", stats->gpus[i].temperature_c);
            printk("[VM-STATS]     Fan: %u%%\n", stats->gpus[i].fan_speed_percent);
        }
    }
    
    /* Processes */
    printk("[VM-STATS] Processes:\n");
    printk("[VM-STATS]   Count: %u\n", stats->process_count);
    printk("[VM-STATS]   Threads: %u\n", stats->thread_count);
    printk("[VM-STATS]   Handles: %u\n", stats->handle_count);
    
    /* Uptime */
    char uptime_str[64];
    vm_format_uptime(stats->uptime_seconds, uptime_str, 64);
    printk("[VM-STATS] Uptime: %s\n", uptime_str);
    
    /* Snapshots */
    printk("[VM-STATS] Snapshots: %u (Total: %lu GB)\n",
           stats->snapshot_count, stats->snapshot_total_size_gb);
    
    /* Extensions */
    printk("[VM-STATS] Extensions: %u\n", stats->extension_count);
    
    /* Health */
    u32 health = vm_calculate_health(stats);
    printk("[VM-STATS] Health: %s\n",
           health == 0 ? "Good ✓" :
           health == 1 ? "Warning ⚠" : "Critical ✗");
    
    printk("[VM-STATS] ========================================\n\n");
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int vm_manager_enhanced_init(void)
{
    printk("[VM-ENHANCED] ========================================\n");
    printk("[VM-ENHANCED] NEXUS OS VM Manager Enhanced\n");
    printk("[VM-ENHANCED] With Detailed Stats & Extensions\n");
    printk("[VM-ENHANCED] ========================================\n");
    
    memset(&g_vm_enhanced, 0, sizeof(vm_manager_enhanced_t));
    g_vm_enhanced.display_mode = 1;  /* Grid view */
    g_vm_enhanced.sort_by = 0;  /* Sort by name */
    g_vm_enhanced.show_all_stats = true;
    g_vm_enhanced.auto_refresh = true;
    g_vm_enhanced.refresh_interval_seconds = 5;
    
    /* Initialize default extensions */
    vm_extension_init_defaults();
    
    printk("[VM-ENHANCED] Display mode: Grid\n");
    printk("[VM-ENHANCED] Auto refresh: Every %u seconds\n", g_vm_enhanced.refresh_interval_seconds);
    printk("[VM-ENHANCED] Extensions: %u loaded\n", g_vm_enhanced.extension_count);
    printk("[VM-ENHANCED] ========================================\n");
    
    return 0;
}

/* Print VM Manager Enhanced Stats */
void vm_manager_enhanced_print_stats(void)
{
    printk("\n[VM-ENHANCED] ====== MANAGER STATS ======\n");
    printk("[VM-ENHANCED] VMs Displayed: %u / 64\n", g_vm_enhanced.card_count);
    printk("[VM-ENHANCED] Extensions: %u / 32\n", g_vm_enhanced.extension_count);
    printk("[VM-ENHANCED] Display Mode: %u\n", g_vm_enhanced.display_mode);
    printk("[VM-ENHANCED] Sort By: %u\n", g_vm_enhanced.sort_by);
    printk("[VM-ENHANCED] Auto Refresh: %s\n", g_vm_enhanced.auto_refresh ? "Yes" : "No");
    printk("[VM-ENHANCED] =============================\n\n");
}
