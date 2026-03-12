/*
 * NEXUS OS - System Registry Implementation
 * system/registry/registry.c
 * 
 * Copyright (c) 2026 NEXUS Development Team
 * 
 * Central configuration database for system and applications
 */

#include "registry.h"

static registry_t g_registry;

/*===========================================================================*/
/*                         REGISTRY INITIALIZATION                           */
/*===========================================================================*/

/**
 * registry_init - Initialize registry system
 */
int registry_init(registry_t *reg)
{
    if (!reg) return -1;
    
    printk("[REGISTRY] Initializing registry system...\n");
    
    memset(reg, 0, sizeof(registry_t));
    spin_lock_init(&reg->lock);
    reg->key_counter = 0;
    reg->dirty = false;
    reg->auto_backup = true;
    
    /* Initialize root keys */
    const char *root_names[] = {
        "ROOT", "CLASSES_ROOT", "CURRENT_USER", 
        "LOCAL_MACHINE", "USERS", "CURRENT_CONFIG"
    };
    
    for (int i = 0; i < 6; i++) {
        registry_key_t *key = &reg->root_keys[i];
        memset(key, 0, sizeof(registry_key_t));
        snprintf(key->path, sizeof(key->path), "\\%s", root_names[i]);
        strncpy(key->name, root_names[i], sizeof(key->name) - 1);
        key->id = i + 1;
        key->parent_id = 0;
        key->created = get_time_ms();
        key->modified = key->created;
        key->values = NULL;
        key->value_count = 0;
        key->subkeys = NULL;
        key->subkey_count = 0;
    }
    
    reg->stats.total_keys = 6;
    reg->stats.total_values = 0;
    
    printk("[REGISTRY] Initialized with %d root keys\n", 6);
    printk("[REGISTRY] Root keys:\n");
    printk("  - HKEY_CLASSES_ROOT\n");
    printk("  - HKEY_CURRENT_USER\n");
    printk("  - HKEY_LOCAL_MACHINE\n");
    printk("  - HKEY_USERS\n");
    printk("  - HKEY_CURRENT_CONFIG\n");
    
    return 0;
}

/**
 * registry_shutdown - Shutdown registry system
 */
int registry_shutdown(registry_t *reg)
{
    if (!reg) return -1;
    
    printk("[REGISTRY] Shutting down...\n");
    
    if (reg->dirty) {
        registry_save(reg, "/etc/nexus/registry.db");
    }
    
    return 0;
}

/**
 * registry_load - Load registry from file
 */
int registry_load(registry_t *reg, const char *path)
{
    if (!reg || !path) return -1;
    
    printk("[REGISTRY] Loading from: %s\n", path);
    
    /* TODO: Load registry from file */
    
    return 0;
}

/**
 * registry_save - Save registry to file
 */
int registry_save(registry_t *reg, const char *path)
{
    if (!reg || !path) return -1;
    
    printk("[REGISTRY] Saving to: %s\n", path);
    
    /* TODO: Save registry to file */
    reg->dirty = false;
    
    return 0;
}

/*===========================================================================*/
/*                         KEY OPERATIONS                                    */
/*===========================================================================*/

/**
 * registry_create_key - Create registry key
 */
int registry_create_key(registry_t *reg, u32 root_key, const char *path)
{
    if (!reg || !path || root_key >= 6) return -1;
    
    spin_lock(&reg->lock);
    
    printk("[REGISTRY] Creating key: HK_%u\\%s\n", root_key, path);
    
    /* TODO: Implement key creation */
    
    reg->stats.total_keys++;
    reg->dirty = true;
    
    spin_unlock(&reg->lock);
    return 0;
}

/**
 * registry_open_key - Open registry key
 */
int registry_open_key(registry_t *reg, u32 root_key, const char *path)
{
    if (!reg || !path || root_key >= 6) return -1;
    
    spin_lock(&reg->lock);
    reg->stats.reads++;
    spin_unlock(&reg->lock);
    
    /* TODO: Implement key opening */
    
    return 0;
}

/**
 * registry_delete_key - Delete registry key
 */
int registry_delete_key(registry_t *reg, u32 root_key, const char *path)
{
    if (!reg || !path || root_key >= 6) return -1;
    
    spin_lock(&reg->lock);
    
    printk("[REGISTRY] Deleting key: HK_%u\\%s\n", root_key, path);
    
    /* TODO: Implement key deletion */
    
    reg->stats.deletes++;
    reg->dirty = true;
    
    spin_unlock(&reg->lock);
    return 0;
}

/*===========================================================================*/
/*                         VALUE OPERATIONS                                  */
/*===========================================================================*/

/**
 * registry_set_value - Set registry value
 */
int registry_set_value(registry_t *reg, u32 root_key, const char *key_path,
                       const char *value_name, int type, const void *data, size_t size)
{
    if (!reg || !key_path || !value_name || !data || root_key >= 6) return -1;
    
    spin_lock(&reg->lock);
    
    /* TODO: Implement value setting */
    
    reg->stats.writes++;
    reg->dirty = true;
    
    spin_unlock(&reg->lock);
    return 0;
}

/**
 * registry_get_value - Get registry value
 */
int registry_get_value(registry_t *reg, u32 root_key, const char *key_path,
                       const char *value_name, int *type, void *data, size_t *size)
{
    if (!reg || !key_path || !value_name || root_key >= 6) return -1;
    
    spin_lock(&reg->lock);
    reg->stats.reads++;
    spin_unlock(&reg->lock);
    
    /* TODO: Implement value retrieval */
    
    return 0;
}

/**
 * registry_set_string - Set string value
 */
int registry_set_string(registry_t *reg, u32 root_key, const char *key_path,
                        const char *value_name, const char *value)
{
    if (!value) return -1;
    return registry_set_value(reg, root_key, key_path, value_name, 
                              REG_STRING, value, strlen(value) + 1);
}

/**
 * registry_get_string - Get string value
 */
int registry_get_string(registry_t *reg, u32 root_key, const char *key_path,
                        const char *value_name, char *value, size_t max_len)
{
    int type;
    size_t size = max_len;
    return registry_get_value(reg, root_key, key_path, value_name, 
                              &type, value, &size);
}

/**
 * registry_set_int - Set integer value
 */
int registry_set_int(registry_t *reg, u32 root_key, const char *key_path,
                     const char *value_name, int value)
{
    return registry_set_value(reg, root_key, key_path, value_name,
                              REG_INT, &value, sizeof(int));
}

/**
 * registry_get_int - Get integer value
 */
int registry_get_int(registry_t *reg, u32 root_key, const char *key_path,
                     const char *value_name, int *value)
{
    int type;
    size_t size = sizeof(int);
    return registry_get_value(reg, root_key, key_path, value_name,
                              &type, value, &size);
}

/**
 * registry_set_int64 - Set 64-bit integer value
 */
int registry_set_int64(registry_t *reg, u32 root_key, const char *key_path,
                       const char *value_name, u64 value)
{
    return registry_set_value(reg, root_key, key_path, value_name,
                              REG_INT64, &value, sizeof(u64));
}

/**
 * registry_get_int64 - Get 64-bit integer value
 */
int registry_get_int64(registry_t *reg, u32 root_key, const char *key_path,
                       const char *value_name, u64 *value)
{
    int type;
    size_t size = sizeof(u64);
    return registry_get_value(reg, root_key, key_path, value_name,
                              &type, value, &size);
}

/**
 * registry_set_binary - Set binary value
 */
int registry_set_binary(registry_t *reg, u32 root_key, const char *key_path,
                        const char *value_name, const void *data, size_t size)
{
    return registry_set_value(reg, root_key, key_path, value_name,
                              REG_BINARY, data, size);
}

/**
 * registry_get_binary - Get binary value
 */
int registry_get_binary(registry_t *reg, u32 root_key, const char *key_path,
                        const char *value_name, void *data, size_t *size)
{
    int type;
    return registry_get_value(reg, root_key, key_path, value_name,
                              &type, data, size);
}

/*===========================================================================*/
/*                         STATISTICS                                        */
/*===========================================================================*/

/**
 * registry_get_stats - Get registry statistics
 */
int registry_get_stats(registry_t *reg, void *stats)
{
    if (!reg || !stats) return -1;
    
    spin_lock(&reg->lock);
    memcpy(stats, &reg->stats, sizeof(reg->stats));
    spin_unlock(&reg->lock);
    
    return 0;
}

/**
 * registry_reset_stats - Reset statistics
 */
int registry_reset_stats(registry_t *reg)
{
    if (!reg) return -1;
    
    spin_lock(&reg->lock);
    memset(&reg->stats, 0, sizeof(reg->stats));
    spin_unlock(&reg->lock);
    
    return 0;
}

/*===========================================================================*/
/*                         MAINTENANCE                                       */
/*===========================================================================*/

/**
 * registry_compact - Compact registry database
 */
int registry_compact(registry_t *reg)
{
    if (!reg) return -1;
    
    printk("[REGISTRY] Compacting database...\n");
    
    /* TODO: Implement compaction */
    
    return 0;
}

/**
 * registry_check - Check registry integrity
 */
int registry_check(registry_t *reg)
{
    if (!reg) return -1;
    
    printk("[REGISTRY] Checking integrity...\n");
    
    /* TODO: Implement integrity check */
    
    return 0;
}

/**
 * registry_repair - Repair registry database
 */
int registry_repair(registry_t *reg)
{
    if (!reg) return -1;
    
    printk("[REGISTRY] Repairing database...\n");
    
    /* TODO: Implement repair */
    
    return 0;
}

/*===========================================================================*/
/*                         DEFAULT KEYS                                      */
/*===========================================================================*/

/**
 * registry_init_defaults - Initialize default registry keys
 */
int registry_init_defaults(registry_t *reg)
{
    if (!reg) return -1;
    
    printk("[REGISTRY] Initializing default keys...\n");
    
    /* System keys */
    registry_create_key(reg, HKEY_LOCAL_MACHINE, "SYSTEM");
    registry_create_key(reg, HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet");
    registry_create_key(reg, HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Services");
    registry_create_key(reg, HKEY_LOCAL_MACHINE, "SOFTWARE");
    registry_create_key(reg, HKEY_LOCAL_MACHINE, "SOFTWARE\\NEXUS");
    registry_create_key(reg, HKEY_LOCAL_MACHINE, "SOFTWARE\\NEXUS\\CurrentVersion");
    
    /* User keys */
    registry_create_key(reg, HKEY_CURRENT_USER, "SOFTWARE");
    registry_create_key(reg, HKEY_CURRENT_USER, "SOFTWARE\\NEXUS");
    registry_create_key(reg, HKEY_CURRENT_USER, "SOFTWARE\\NEXUS\\Desktop");
    registry_create_key(reg, HKEY_CURRENT_USER, "SOFTWARE\\NEXUS\\Explorer");
    
    /* Set some default values */
    registry_set_string(reg, HKEY_LOCAL_MACHINE, 
        "SOFTWARE\\NEXUS\\CurrentVersion", "Version", "1.0.0");
    registry_set_string(reg, HKEY_LOCAL_MACHINE,
        "SOFTWARE\\NEXUS\\CurrentVersion", "ProductName", "NEXUS OS");
    registry_set_int(reg, HKEY_CURRENT_USER,
        "SOFTWARE\\NEXUS\\Desktop", "IconSize", 64);
    registry_set_int(reg, HKEY_CURRENT_USER,
        "SOFTWARE\\NEXUS\\Desktop", "GridSize", 80);
    
    return 0;
}

/*===========================================================================*/
/*                         GLOBAL REGISTRY ACCESS                            */
/*===========================================================================*/

/**
 * registry_get - Get global registry instance
 */
registry_t *registry_get(void)
{
    return &g_registry;
}

/**
 * registry_init_global - Initialize global registry
 */
int registry_init_global(void)
{
    int ret = registry_init(&g_registry);
    if (ret == 0) {
        registry_init_defaults(&g_registry);
    }
    return ret;
}

/**
 * registry_shutdown_global - Shutdown global registry
 */
int registry_shutdown_global(void)
{
    return registry_shutdown(&g_registry);
}
