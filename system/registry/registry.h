/*
 * NEXUS OS - System Registry
 * system/registry/registry.h
 * 
 * Central configuration database for system and applications
 */

#ifndef _NEXUS_REGISTRY_H
#define _NEXUS_REGISTRY_H

#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         REGISTRY CONSTANTS                                */
/*===========================================================================*/

#define REGISTRY_MAX_PATH           1024
#define REGISTRY_MAX_VALUE_NAME     256
#define REGISTRY_MAX_VALUE_SIZE     (1024 * 1024)  /* 1MB max value */
#define REGISTRY_MAX_KEYS           65536

/* Registry value types */
#define REG_NONE                    0
#define REG_STRING                  1
#define REG_INT                     2
#define REG_INT64                   3
#define REG_BINARY                  4
#define REG_ARRAY                   5
#define REG_PATH                    6

/* Root keys */
#define HKEY_ROOT                   0
#define HKEY_CLASSES_ROOT           1
#define HKEY_CURRENT_USER           2
#define HKEY_LOCAL_MACHINE          3
#define HKEY_USERS                  4
#define HKEY_CURRENT_CONFIG         5

/* Registry flags */
#define REG_FLAG_READONLY           0x00000001
#define REG_FLAG_VOLATILE           0x00000002
#define REG_FLAG_ENCRYPTED          0x00000004
#define REG_FLAG_SYSTEM             0x00000008

/*===========================================================================*/
/*                         REGISTRY STRUCTURES                               */
/*===========================================================================*/

/**
 * registry_value_t - Registry value
 */
typedef struct registry_value {
    char name[REGISTRY_MAX_VALUE_NAME];
    int type;
    void *data;
    size_t size;
    u64 modified;
    u32 flags;
} registry_value_t;

/**
 * registry_key_t - Registry key
 */
typedef struct registry_key {
    char path[REGISTRY_MAX_PATH];
    char name[256];
    u32 id;
    u32 parent_id;
    u64 created;
    u64 modified;
    u32 flags;
    
    /* Values */
    registry_value_t *values;
    int value_count;
    
    /* Subkeys */
    struct registry_key *subkeys;
    int subkey_count;
    
    /* Security */
    u32 owner;
    u32 permissions;
} registry_key_t;

/**
 * registry_t - Registry instance
 */
typedef struct {
    /* Root keys */
    registry_key_t root_keys[6];
    
    /* Key counter */
    u32 key_counter;
    
    /* Dirty flag */
    bool dirty;
    
    /* Lock */
    spinlock_t lock;
    
    /* Statistics */
    struct {
        u32 total_keys;
        u32 total_values;
        u32 reads;
        u32 writes;
        u32 deletes;
    } stats;
    
    /* Backup */
    char backup_path[256];
    bool auto_backup;
    u64 last_backup;
} registry_t;

/**
 * registry_watch_t - Registry change watch
 */
typedef struct {
    char path[REGISTRY_MAX_PATH];
    bool recursive;
    void (*callback)(const char *path, const char *value, int type);
    void *user_data;
} registry_watch_t;

/*===========================================================================*/
/*                         REGISTRY API                                      */
/*===========================================================================*/

/* Initialization */
int registry_init(registry_t *reg);
int registry_shutdown(registry_t *reg);
int registry_load(registry_t *reg, const char *path);
int registry_save(registry_t *reg, const char *path);
int registry_backup(registry_t *reg);
int registry_restore(registry_t *reg, const char *backup_path);

/* Key operations */
int registry_create_key(registry_t *reg, u32 root_key, const char *path);
int registry_open_key(registry_t *reg, u32 root_key, const char *path);
int registry_delete_key(registry_t *reg, u32 root_key, const char *path);
int registry_copy_key(registry_t *reg, u32 root_key, const char *src_path, const char *dst_path);
int registry_move_key(registry_t *reg, u32 root_key, const char *src_path, const char *dst_path);
bool registry_key_exists(registry_t *reg, u32 root_key, const char *path);
int registry_enum_keys(registry_t *reg, u32 root_key, const char *path, char **keys, int *count);

/* Value operations */
int registry_set_value(registry_t *reg, u32 root_key, const char *key_path, 
                       const char *value_name, int type, const void *data, size_t size);
int registry_get_value(registry_t *reg, u32 root_key, const char *key_path,
                       const char *value_name, int *type, void *data, size_t *size);
int registry_delete_value(registry_t *reg, u32 root_key, const char *key_path, const char *value_name);
bool registry_value_exists(registry_t *reg, u32 root_key, const char *key_path, const char *value_name);
int registry_enum_values(registry_t *reg, u32 root_key, const char *key_path, 
                         char **names, int *types, int *count);

/* Typed value operations */
int registry_set_string(registry_t *reg, u32 root_key, const char *key_path,
                        const char *value_name, const char *value);
int registry_get_string(registry_t *reg, u32 root_key, const char *key_path,
                        const char *value_name, char *value, size_t max_len);
int registry_set_int(registry_t *reg, u32 root_key, const char *key_path,
                     const char *value_name, int value);
int registry_get_int(registry_t *reg, u32 root_key, const char *key_path,
                     const char *value_name, int *value);
int registry_set_int64(registry_t *reg, u32 root_key, const char *key_path,
                       const char *value_name, u64 value);
int registry_get_int64(registry_t *reg, u32 root_key, const char *key_path,
                       const char *value_name, u64 *value);
int registry_set_binary(registry_t *reg, u32 root_key, const char *key_path,
                        const char *value_name, const void *data, size_t size);
int registry_get_binary(registry_t *reg, u32 root_key, const char *key_path,
                        const char *value_name, void *data, size_t *size);

/* Path operations */
int registry_get_full_path(u32 root_key, const char *key_path, char *full_path, size_t max_len);
int registry_parse_path(const char *full_path, u32 *root_key, char **key_path);

/* Watch operations */
int registry_watch(registry_t *reg, const char *path, bool recursive, 
                   void (*callback)(const char *path, const char *value, int type), void *user_data);
int registry_unwatch(registry_t *reg, const char *path);

/* Security */
int registry_set_permissions(registry_t *reg, u32 root_key, const char *key_path, u32 permissions);
int registry_get_permissions(registry_t *reg, u32 root_key, const char *key_path, u32 *permissions);
int registry_set_owner(registry_t *reg, u32 root_key, const char *key_path, u32 owner);

/* Export/Import */
int registry_export(registry_t *reg, u32 root_key, const char *key_path, const char *file_path);
int registry_import(registry_t *reg, u32 root_key, const char *file_path);

/* Query */
int registry_get_key_info(registry_t *reg, u32 root_key, const char *key_path, 
                          u64 *created, u64 *modified, int *subkey_count, int *value_count);
int registry_get_value_info(registry_t *reg, u32 root_key, const char *key_path, 
                            const char *value_name, int *type, size_t *size, u64 *modified);

/* Statistics */
int registry_get_stats(registry_t *reg, void *stats);
int registry_reset_stats(registry_t *reg);

/* Maintenance */
int registry_compact(registry_t *reg);
int registry_check(registry_t *reg);
int registry_repair(registry_t *reg);

/* Default keys */
int registry_init_defaults(registry_t *reg);
int registry_init_system_keys(registry_t *reg);
int registry_init_user_keys(registry_t *reg);

#endif /* _NEXUS_REGISTRY_H */
