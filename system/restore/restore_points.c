/*
 * NEXUS OS - System Restore Points
 * system/restore/restore_points.c
 *
 * System restore, snapshots, rollback support
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/* Forward declarations */
int restore_create(const char *name, const char *description, u32 type, bool protected);

/*===========================================================================*/
/*                         RESTORE POINTS CONFIGURATION                      */
/*===========================================================================*/

#define RESTORE_MAX_POINTS        32
#define RESTORE_MAX_SNAPSHOTS     16
#define RESTORE_MAX_SIZE_GB       50

/* Restore Point Types */
#define RESTORE_TYPE_MANUAL       1
#define RESTORE_TYPE_AUTO         2
#define RESTORE_TYPE_INSTALL      3
#define RESTORE_TYPE_UPDATE       4
#define RESTORE_TYPE_DRIVER       5

/* Restore Point States */
#define RESTORE_STATE_VALID       0
#define RESTORE_STATE_CORRUPT     1
#define RESTORE_STATE_INCOMPLETE  2

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 id;
    char name[128];
    char description[256];
    u32 type;
    u32 state;
    u64 created_time;
    u64 size;
    char trigger_event[64];     /* What triggered this restore point */
    char system_version[32];
    u32 files_changed;
    u32 registry_changes;
    bool is_protected;          /* Don't auto-delete */
    bool is_valid;
} restore_point_t;

typedef struct {
    u32 snapshot_id;
    u32 restore_point_id;
    char path[256];
    u64 size;
    u32 checksum;
    bool is_compressed;
} restore_snapshot_t;

typedef struct {
    bool initialized;
    restore_point_t points[RESTORE_MAX_POINTS];
    u32 point_count;
    u32 next_id;
    restore_snapshot_t snapshots[RESTORE_MAX_SNAPSHOTS];
    u32 snapshot_count;
    u64 total_storage_used;
    u32 max_storage_gb;
    bool is_creating;
    bool is_restoring;
    u32 restores_performed;
    u64 last_restore_time;
    spinlock_t lock;
} restore_manager_t;

static restore_manager_t g_restore;

/*===========================================================================*/
/*                         RESTORE MANAGER CORE                              */
/*===========================================================================*/

int restore_init(void)
{
    printk("[RESTORE] ========================================\n");
    printk("[RESTORE] NEXUS OS System Restore\n");
    printk("[RESTORE] ========================================\n");
    
    memset(&g_restore, 0, sizeof(restore_manager_t));
    spinlock_init(&g_restore.lock);
    
    g_restore.next_id = 1;
    g_restore.max_storage_gb = RESTORE_MAX_SIZE_GB;
    
    /* Create initial system restore point */
    restore_create("Initial System State", 
                   "System installation complete",
                   RESTORE_TYPE_INSTALL,
                   true);  /* Protected */
    
    printk("[RESTORE] Restore manager initialized\n");
    printk("[RESTORE]   Max storage: %d GB\n", g_restore.max_storage_gb);
    
    return 0;
}

/*===========================================================================*/
/*                         RESTORE POINT CREATION                            */
/*===========================================================================*/

int restore_create(const char *name, const char *description, u32 type, bool protected)
{
    if (g_restore.point_count >= RESTORE_MAX_POINTS) {
        /* Delete oldest non-protected point */
        for (u32 i = 0; i < g_restore.point_count; i++) {
            if (!g_restore.points[i].is_protected) {
                restore_delete(g_restore.points[i].id);
                break;
            }
        }
    }
    
    if (g_restore.point_count >= RESTORE_MAX_POINTS) {
        printk("[RESTORE] Maximum restore points reached\n");
        return -ENOMEM;
    }
    
    g_restore.is_creating = true;
    
    restore_point_t *point = &g_restore.points[g_restore.point_count++];
    memset(point, 0, sizeof(restore_point_t));
    
    point->id = g_restore.next_id++;
    strncpy(point->name, name, sizeof(point->name) - 1);
    strncpy(point->description, description, sizeof(point->description) - 1);
    point->type = type;
    point->state = RESTORE_STATE_VALID;
    point->created_time = ktime_get_sec();
    point->is_protected = protected;
    point->is_valid = true;
    
    /* Get system version */
    strcpy(point->system_version, "1.0.0");
    
    /* Create snapshots of critical system areas */
    restore_snapshot_t *snap;
    
    /* System files snapshot */
    snap = &g_restore.snapshots[g_restore.snapshot_count++];
    snap->snapshot_id = g_restore.snapshot_count;
    snap->restore_point_id = point->id;
    strcpy(snap->path, "/system");
    snap->is_compressed = true;
    snap->size = 5ULL * 1024 * 1024 * 1024;  /* 5GB estimated */
    point->size += snap->size;
    
    /* Registry snapshot */
    snap = &g_restore.snapshots[g_restore.snapshot_count++];
    snap->snapshot_id = g_restore.snapshot_count;
    snap->restore_point_id = point->id;
    strcpy(snap->path, "/registry");
    snap->is_compressed = true;
    snap->size = 100 * 1024 * 1024;  /* 100MB */
    point->size += snap->size;
    
    /* Driver store snapshot */
    snap = &g_restore.snapshots[g_restore.snapshot_count++];
    snap->snapshot_id = g_restore.snapshot_count;
    snap->restore_point_id = point->id;
    strcpy(snap->path, "/drivers");
    snap->is_compressed = true;
    snap->size = 2ULL * 1024 * 1024 * 1024;  /* 2GB */
    point->size += snap->size;
    
    g_restore.total_storage_used += point->size;
    g_restore.is_creating = false;
    
    const char *type_names[] = {"", "Manual", "Automatic", "Install", "Update", "Driver"};
    
    printk("[RESTORE] Created restore point #%d: %s\n", point->id, point->name);
    printk("[RESTORE]   Type: %s\n", type_names[type]);
    printk("[RESTORE]   Size: %d MB\n", (u32)(point->size / (1024 * 1024)));
    printk("[RESTORE]   Snapshots: %d\n", 3);
    
    return point->id;
}

/*===========================================================================*/
/*                         RESTORE POINT MANAGEMENT                          */
/*===========================================================================*/

int restore_delete(u32 point_id)
{
    s32 point_idx = -1;
    
    for (u32 i = 0; i < g_restore.point_count; i++) {
        if (g_restore.points[i].id == point_id) {
            point_idx = i;
            break;
        }
    }
    
    if (point_idx < 0) return -ENOENT;
    
    restore_point_t *point = &g_restore.points[point_idx];
    
    if (point->is_protected) {
        printk("[RESTORE] Cannot delete protected restore point #%d\n", point_id);
        return -EPERM;
    }
    
    /* Delete associated snapshots */
    for (s32 i = g_restore.snapshot_count - 1; i >= 0; i--) {
        if (g_restore.snapshots[i].restore_point_id == point_id) {
            g_restore.total_storage_used -= g_restore.snapshots[i].size;
            
            /* Shift snapshots */
            for (u32 j = i; j < g_restore.snapshot_count - 1; j++) {
                g_restore.snapshots[j] = g_restore.snapshots[j + 1];
            }
            g_restore.snapshot_count--;
        }
    }
    
    g_restore.total_storage_used -= point->size;
    
    /* Shift points */
    for (u32 i = point_idx; i < g_restore.point_count - 1; i++) {
        g_restore.points[i] = g_restore.points[i + 1];
    }
    g_restore.point_count--;
    
    printk("[RESTORE] Deleted restore point #%d\n", point_id);
    
    return 0;
}

int restore_list(restore_point_t *points, u32 *count)
{
    if (!points || !count) return -EINVAL;
    
    u32 copy = (*count < g_restore.point_count) ? *count : g_restore.point_count;
    memcpy(points, g_restore.points, sizeof(restore_point_t) * copy);
    *count = copy;
    
    return copy;
}

restore_point_t *restore_get(u32 point_id)
{
    for (u32 i = 0; i < g_restore.point_count; i++) {
        if (g_restore.points[i].id == point_id) {
            return &g_restore.points[i];
        }
    }
    return NULL;
}

/*===========================================================================*/
/*                         SYSTEM RESTORE                                    */
/*===========================================================================*/

int restore_system(u32 point_id)
{
    restore_point_t *point = restore_get(point_id);
    if (!point) return -ENOENT;
    
    if (point->state != RESTORE_STATE_VALID) {
        printk("[RESTORE] Restore point #%d is not valid\n", point_id);
        return -EINVAL;
    }
    
    printk("[RESTORE] ========================================\n");
    printk("[RESTORE] Starting system restore...\n");
    printk("[RESTORE] Restore Point: %s\n", point->name);
    printk("[RESTORE] Created: %d seconds ago\n", 
           (u32)(ktime_get_sec() - point->created_time));
    printk("[RESTORE] ========================================\n");
    
    g_restore.is_restoring = true;
    
    /* In real implementation:
     * 1. Boot into recovery mode
     * 2. Load snapshots
     * 3. Restore system files
     * 4. Restore registry
     * 5. Restore drivers
     * 6. Reboot
     */
    
    /* Mock: simulate restore */
    printk("[RESTORE] Restoring system files...\n");
    printk("[RESTORE] Restoring registry...\n");
    printk("[RESTORE] Restoring drivers...\n");
    
    g_restore.is_restoring = false;
    g_restore.restores_performed++;
    g_restore.last_restore_time = ktime_get_sec();
    
    printk("[RESTORE] System restore complete!\n");
    printk("[RESTORE] Please reboot your system.\n");
    
    return 0;
}

/*===========================================================================*/
/*                         AUTOMATIC RESTORE POINTS                          */
/*===========================================================================*/

int restore_auto_create_before_install(const char *software_name)
{
    char name[128];
    char desc[256];
    
    snprintf(name, sizeof(name), "Before %s Installation", software_name);
    snprintf(desc, sizeof(desc), "Automatic restore point before installing %s", software_name);
    
    return restore_create(name, desc, RESTORE_TYPE_INSTALL, false);
}

int restore_auto_create_before_update(const char *update_name)
{
    char name[128];
    char desc[256];
    
    snprintf(name, sizeof(name), "Before %s Update", update_name);
    snprintf(desc, sizeof(desc), "Automatic restore point before updating %s", update_name);
    
    return restore_create(name, desc, RESTORE_TYPE_UPDATE, false);
}

int restore_auto_create_before_driver(const char *driver_name)
{
    char name[128];
    char desc[256];
    
    snprintf(name, sizeof(name), "Before %s Driver", driver_name);
    snprintf(desc, sizeof(desc), "Automatic restore point before installing %s driver", driver_name);
    
    return restore_create(name, desc, RESTORE_TYPE_DRIVER, false);
}

/*===========================================================================*/
/*                         STORAGE MANAGEMENT                                */
/*===========================================================================*/

int restore_set_max_storage(u32 max_gb)
{
    if (max_gb < 5 || max_gb > 200) return -EINVAL;
    
    g_restore.max_storage_gb = max_gb;
    
    /* Delete old points if over limit */
    while (g_restore.total_storage_used > (u64)max_gb * 1024 * 1024 * 1024) {
        /* Find oldest non-protected point */
        u32 oldest_id = 0;
        u64 oldest_time = ~0ULL;
        
        for (u32 i = 0; i < g_restore.point_count; i++) {
            if (!g_restore.points[i].is_protected &&
                g_restore.points[i].created_time < oldest_time) {
                oldest_time = g_restore.points[i].created_time;
                oldest_id = g_restore.points[i].id;
            }
        }
        
        if (oldest_id == 0) break;  /* All protected */
        
        restore_delete(oldest_id);
    }
    
    printk("[RESTORE] Max storage set to %d GB\n", max_gb);
    return 0;
}

int restore_get_storage_info(u64 *used, u64 *max, u32 *point_count)
{
    if (used) *used = g_restore.total_storage_used;
    if (max) *max = (u64)g_restore.max_storage_gb * 1024 * 1024 * 1024;
    if (point_count) *point_count = g_restore.point_count;
    
    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

int restore_get_stats(u32 *total_points, u32 *restores, u64 *storage_used)
{
    if (total_points) *total_points = g_restore.point_count;
    if (restores) *restores = g_restore.restores_performed;
    if (storage_used) *storage_used = g_restore.total_storage_used;
    
    return 0;
}

restore_manager_t *restore_manager_get(void)
{
    return &g_restore;
}
