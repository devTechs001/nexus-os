/*
 * NEXUS OS - Cache Cleaner
 * system/cleanup/cache_cleaner.c
 *
 * System cache cleaning, temp files, browser cache
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         CACHE CLEANER CONFIGURATION                       */
/*===========================================================================*/

#define CACHE_MAX_LOCATIONS       32
#define CACHE_MAX_STATS           100

/* Cache Types */
#define CACHE_TYPE_SYSTEM         1
#define CACHE_TYPE_BROWSER        2
#define CACHE_TYPE_APPLICATION    3
#define CACHE_TYPE_THUMBNAIL      4
#define CACHE_TYPE_LOG            5
#define CACHE_TYPE_TEMP           6

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 id;
    char name[128];
    char path[256];
    u32 type;
    u64 size;
    u32 file_count;
    u64 last_cleaned;
    bool is_enabled;
    bool auto_clean;
} cache_location_t;

typedef struct {
    u64 total_cleaned;
    u64 last_clean_size;
    u32 clean_count;
    u64 last_clean_time;
    u32 files_deleted;
    u64 space_saved;
} cache_stats_t;

typedef struct {
    bool initialized;
    cache_location_t locations[CACHE_MAX_LOCATIONS];
    u32 location_count;
    cache_stats_t stats;
    bool is_cleaning;
    u64 cleaning_start_time;
    spinlock_t lock;
} cache_cleaner_t;

static cache_cleaner_t g_cache;

/*===========================================================================*/
/*                         CACHE CLEANER CORE                                */
/*===========================================================================*/

int cache_cleaner_init(void)
{
    printk("[CACHE] ========================================\n");
    printk("[CACHE] NEXUS OS Cache Cleaner\n");
    printk("[CACHE] ========================================\n");
    
    memset(&g_cache, 0, sizeof(cache_cleaner_t));
    spinlock_init(&g_cache.lock);
    
    /* Register cache locations */
    cache_location_t *loc;
    
    /* System cache */
    loc = &g_cache.locations[g_cache.location_count++];
    loc->id = 1;
    strcpy(loc->name, "System Cache");
    strcpy(loc->path, "/var/cache");
    loc->type = CACHE_TYPE_SYSTEM;
    loc->is_enabled = true;
    loc->auto_clean = true;
    
    /* Temp files */
    loc = &g_cache.locations[g_cache.location_count++];
    loc->id = 2;
    strcpy(loc->name, "Temporary Files");
    strcpy(loc->path, "/tmp");
    loc->type = CACHE_TYPE_TEMP;
    loc->is_enabled = true;
    loc->auto_clean = true;
    
    /* Application cache */
    loc = &g_cache.locations[g_cache.location_count++];
    loc->id = 3;
    strcpy(loc->name, "Application Cache");
    strcpy(loc->path, "/var/cache/app");
    loc->type = CACHE_TYPE_APPLICATION;
    loc->is_enabled = true;
    loc->auto_clean = false;
    
    /* Thumbnail cache */
    loc = &g_cache.locations[g_cache.location_count++];
    loc->id = 4;
    strcpy(loc->name, "Thumbnail Cache");
    strcpy(loc->path, "/var/cache/thumbnails");
    loc->type = CACHE_TYPE_THUMBNAIL;
    loc->is_enabled = true;
    loc->auto_clean = true;
    
    /* Log files */
    loc = &g_cache.locations[g_cache.location_count++];
    loc->id = 5;
    strcpy(loc->name, "Log Files");
    strcpy(loc->path, "/var/log");
    loc->type = CACHE_TYPE_LOG;
    loc->is_enabled = true;
    loc->auto_clean = false;
    
    /* Browser cache */
    loc = &g_cache.locations[g_cache.location_count++];
    loc->id = 6;
    strcpy(loc->name, "Browser Cache");
    strcpy(loc->path, "/home/.cache/browser");
    loc->type = CACHE_TYPE_BROWSER;
    loc->is_enabled = true;
    loc->auto_clean = true;
    
    printk("[CACHE] Cache cleaner initialized (%d locations)\n", 
           g_cache.location_count);
    
    return 0;
}

/*===========================================================================*/
/*                         CACHE ANALYSIS                                    */
/*===========================================================================*/

int cache_analyze(u32 location_id, u64 *size, u32 *file_count)
{
    if (location_id >= g_cache.location_count) return -EINVAL;
    
    cache_location_t *loc = &g_cache.locations[location_id];
    
    printk("[CACHE] Analyzing %s...\n", loc->name);
    
    /* In real implementation, would scan directory */
    /* Mock: return estimated sizes */
    u64 mock_sizes[] = {
        500 * 1024 * 1024,    /* System: 500MB */
        100 * 1024 * 1024,    /* Temp: 100MB */
        300 * 1024 * 1024,    /* App: 300MB */
        200 * 1024 * 1024,    /* Thumbnail: 200MB */
        150 * 1024 * 1024,    /* Log: 150MB */
        400 * 1024 * 1024,    /* Browser: 400MB */
    };
    
    loc->size = mock_sizes[location_id];
    loc->file_count = (u32)(loc->size / (4 * 1024));  /* Assume 4KB avg */
    
    if (size) *size = loc->size;
    if (file_count) *file_count = loc->file_count;
    
    printk("[CACHE] %s: %d MB (%d files)\n", 
           loc->name, 
           (u32)(loc->size / (1024 * 1024)),
           loc->file_count);
    
    return 0;
}

int cache_analyze_all(u64 *total_size, u32 *total_files)
{
    u64 total = 0;
    u32 files = 0;
    
    for (u32 i = 0; i < g_cache.location_count; i++) {
        u64 size;
        u32 count;
        cache_analyze(i, &size, &count);
        total += size;
        files += count;
    }
    
    if (total_size) *total_size = total;
    if (total_files) *total_files = files;
    
    printk("[CACHE] Total: %d MB (%d files)\n",
           (u32)(total / (1024 * 1024)), files);
    
    return 0;
}

/*===========================================================================*/
/*                         CACHE CLEANING                                    */
/*===========================================================================*/

int cache_clean_location(u32 location_id)
{
    if (location_id >= g_cache.location_count) return -EINVAL;
    
    cache_location_t *loc = &g_cache.locations[location_id];
    
    if (!loc->is_enabled) {
        printk("[CACHE] %s is disabled\n", loc->name);
        return 0;
    }
    
    printk("[CACHE] Cleaning %s...\n", loc->name);
    
    g_cache.is_cleaning = true;
    g_cache.cleaning_start_time = ktime_get_sec();
    
    u64 freed_space = loc->size;
    u32 deleted_files = loc->file_count;
    
    /* In real implementation, would delete files */
    /* Mock: simulate cleaning */
    
    /* Update stats */
    g_cache.stats.total_cleaned += freed_space;
    g_cache.stats.last_clean_size = freed_space;
    g_cache.stats.clean_count++;
    g_cache.stats.last_clean_time = ktime_get_sec();
    g_cache.stats.files_deleted += deleted_files;
    g_cache.stats.space_saved += freed_space;
    
    /* Reset location stats */
    loc->size = 0;
    loc->file_count = 0;
    loc->last_cleaned = ktime_get_sec();
    
    g_cache.is_cleaning = false;
    
    printk("[CACHE] Cleaned %s: freed %d MB (%d files)\n",
           loc->name,
           (u32)(freed_space / (1024 * 1024)),
           deleted_files);
    
    return 0;
}

int cache_clean_all(void)
{
    printk("[CACHE] Starting full cache cleanup...\n");
    
    u64 total_freed = 0;
    u32 total_deleted = 0;
    
    for (u32 i = 0; i < g_cache.location_count; i++) {
        if (g_cache.locations[i].auto_clean) {
            cache_clean_location(i);
            total_freed += g_cache.locations[i].size;
            total_deleted += g_cache.locations[i].file_count;
        }
    }
    
    printk("[CACHE] Full cleanup complete: freed %d MB\n",
           (u32)(total_freed / (1024 * 1024)));
    
    return 0;
}

/*===========================================================================*/
/*                         AUTO CLEAN CONFIGURATION                          */
/*===========================================================================*/

int cache_set_auto_clean(u32 location_id, bool enabled)
{
    if (location_id >= g_cache.location_count) return -EINVAL;
    
    g_cache.locations[location_id].auto_clean = enabled;
    
    printk("[CACHE] Auto-clean for %s: %s\n",
           g_cache.locations[location_id].name,
           enabled ? "enabled" : "disabled");
    
    return 0;
}

int cache_set_enabled(u32 location_id, bool enabled)
{
    if (location_id >= g_cache.location_count) return -EINVAL;
    
    g_cache.locations[location_id].is_enabled = enabled;
    
    printk("[CACHE] %s: %s\n",
           g_cache.locations[location_id].name,
           enabled ? "enabled" : "disabled");
    
    return 0;
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

int cache_get_stats(cache_stats_t *stats)
{
    if (!stats) return -EINVAL;
    
    *stats = g_cache.stats;
    return 0;
}

int cache_get_location_stats(u32 location_id, u64 *size, u32 *files, u64 *last_cleaned)
{
    if (location_id >= g_cache.location_count) return -EINVAL;
    
    cache_location_t *loc = &g_cache.locations[location_id];
    
    if (size) *size = loc->size;
    if (files) *files = loc->file_count;
    if (last_cleaned) *last_cleaned = loc->last_cleaned;
    
    return 0;
}

cache_cleaner_t *cache_cleaner_get(void)
{
    return &g_cache;
}
