/*
 * NEXUS OS - Storage Encryption Manager
 * security/encryption/encryption-manager.h
 *
 * LUKS disk encryption management with full key management
 * Copyright (c) 2026 NEXUS Development Team
 */

#ifndef _ENCRYPTION_MANAGER_H
#define _ENCRYPTION_MANAGER_H

#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         ENCRYPTION CONFIGURATION                          */
/*===========================================================================*/

#define ENCRYPTION_VERSION          "1.0.0"
#define ENCRYPTION_MAX_DEVICES      16
#define ENCRYPTION_MAX_KEYFILES     8
#define ENCRYPTION_MAX_PASSPHRASE   512
#define ENCRYPTION_MIN_PASSPHRASE   8
#define ENCRYPTION_SALT_SIZE        32
#define ENCRYPTION_KEY_SIZE         64
#define ENCRYPTION_IV_SIZE          16
#define ENCRYPTION_MASTER_KEY_SIZE  512

/*===========================================================================*/
/*                         LUKS HEADER CONSTANTS                             */
/*===========================================================================*/

/* LUKS Magic String */
#define LUKS_MAGIC                  "LUKS\xba\xbe"
#define LUKS_VERSION                "LUKS1"

/* LUKS Header Offsets */
#define LUKS_HEADER_OFFSET          0
#define LUKS_HEADER_SIZE            512
#define LUKS_KEY_MATERIAL_OFFSET    4096
#define LUKS_KEY_MATERIAL_SIZE      4096
#define LUKS_MASTER_KEY_OFFSET      8192

/* LUKS Cipher Modes */
#define LUKS_CIPHER_AES             0
#define LUKS_CIPHER_SERPENT         1
#define LUKS_CIPHER_TWOFISH         2

/* LUKS Cipher Modes */
#define LUKS_MODE_CBC               0
#define LUKS_MODE_ECB               1
#define LUKS_MODE_LRW               2
#define LUKS_MODE_XTS               3
#define LUKS_MODE_ESSIV             4

/* Hash Algorithms */
#define LUKS_HASH_SHA256            0
#define LUKS_HASH_SHA512            1
#define LUKS_HASH_SHA1              2
#define LUKS_HASH_RIPEMD160         3
#define LUKS_HASH_WHIRLPOOL         4

/* Key Slots */
#define LUKS_MAX_KEY_SLOTS          8
#define LUKS_DEFAULT_KEY_SLOT       0

/* Encryption Status */
#define ENCRYPTION_STATUS_NONE      0
#define ENCRYPTION_STATUS_LOCKED    1
#define ENCRYPTION_STATUS_UNLOCKED  2
#define ENCRYPTION_STATUS_ENCRYPTING 3
#define ENCRYPTION_STATUS_DECRYPTING 4

/* Key Types */
#define KEY_TYPE_PASSPHRASE         0
#define KEY_TYPE_KEYFILE            1
#define KEY_TYPE_TOKEN              2
#define KEY_TYPE_HARDWARE           3

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

/**
 * LUKS Header Structure
 */
typedef struct __packed {
    char magic[6];                /* LUKS magic string */
    u16 version;                  /* LUKS version */
    char cipher_name[32];         /* Cipher name (e.g., aes) */
    char cipher_mode[32];         /* Cipher mode (e.g., xts-plain64) */
    char hash_spec[32];           /* Hash specification */
    u32 payload_offset;           /* Offset to encrypted data */
    u32 key_bytes;                /* Key size in bytes */
    u8 master_key_digest[32];     /* Master key digest */
    char salt[40];                /* Salt for master key */
    u32 mk_iterations;            /* Iterations for master key */
    u32 uuid_offset;              /* UUID offset */
    u8 uuid[40];                  /* UUID */
    u8 reserved[192];             /* Reserved */
    struct {
        u32 active;               /* Key slot active flag */
        u32 iterations;           /* PBKDF2 iterations */
        u8 salt[32];              /* Salt for this slot */
        u32 key_offset;           /* Key material offset */
        u32 stripes;              /* Stripes for anti-forensics */
    } key_slots[LUKS_MAX_KEY_SLOTS];
} luks_header_t;

/**
 * Encryption Key
 */
typedef struct {
    u32 key_id;                   /* Key ID */
    u32 key_type;                 /* Key Type */
    char name[64];                /* Key Name */
    u8 key_data[ENCRYPTION_KEY_SIZE]; /* Key Data */
    u32 key_size;                 /* Key Size */
    u8 salt[ENCRYPTION_SALT_SIZE]; /* Salt */
    u32 iterations;               /* PBKDF2 Iterations */
    u64 created_time;             /* Creation Time */
    u64 last_used;                /* Last Used Time */
    u32 use_count;                /* Use Count */
    bool is_enabled;              /* Is Enabled */
    bool is_primary;              /* Is Primary Key */
} encryption_key_t;

/**
 * Encrypted Device
 */
typedef struct encrypted_device {
    u32 device_id;                /* Device ID */
    char name[64];                /* Device Name */
    char device_path[256];        /* Device Path */
    char mapper_path[256];        /* Mapper Path */
    char uuid[64];                /* Device UUID */
    
    /* Encryption Info */
    u32 cipher;                   /* Cipher Algorithm */
    u32 mode;                     /* Cipher Mode */
    u32 hash;                     /* Hash Algorithm */
    u32 key_size;                 /* Key Size (bits) */
    u64 data_offset;              /* Data Offset (sectors) */
    u64 total_sectors;            /* Total Sectors */
    
    /* LUKS Header */
    luks_header_t header;         /* LUKS Header */
    bool has_luks_header;         /* Has LUKS Header */
    
    /* Keys */
    encryption_key_t keys[LUKS_MAX_KEY_SLOTS]; /* Key Slots */
    u32 active_key_slot;          /* Active Key Slot */
    u8 master_key[ENCRYPTION_MASTER_KEY_SIZE]; /* Master Key */
    
    /* Status */
    u32 status;                   /* Encryption Status */
    bool is_encrypted;            /* Is Encrypted */
    bool is_unlocked;             /* Is Unlocked */
    bool is_suspended;            /* Is Suspended */
    
    /* Statistics */
    u64 encrypt_operations;       /* Encrypt Operations */
    u64 decrypt_operations;       /* Decrypt Operations */
    u64 encrypt_bytes;            /* Encrypted Bytes */
    u64 decrypt_bytes;            /* Decrypted Bytes */
    u64 failed_attempts;          /* Failed Attempts */
    
    /* Timing */
    u64 unlock_time;              /* Unlock Time */
    u64 lock_time;                /* Lock Time */
    u64 timeout;                  /* Auto-lock Timeout */
    
    /* Callbacks */
    int (*on_unlock)(struct encrypted_device *);
    int (*on_lock)(struct encrypted_device *);
    int (*on_timeout)(struct encrypted_device *);
    
    /* User Data */
    void *user_data;
    
    /* Links */
    struct encrypted_device *next;
} encrypted_device_t;

/**
 * Encryption Manager State
 */
typedef struct {
    bool initialized;             /* Is Initialized */
    bool hardware_crypto;         /* Hardware Crypto Support */
    
    /* Devices */
    encrypted_device_t *devices;  /* Device List */
    u32 device_count;             /* Device Count */
    
    /* Default Settings */
    u32 default_cipher;           /* Default Cipher */
    u32 default_mode;             /* Default Mode */
    u32 default_hash;             /* Default Hash */
    u32 default_key_size;         /* Default Key Size */
    u32 default_iterations;       /* Default Iterations */
    
    /* Security Settings */
    u32 max_failed_attempts;      /* Max Failed Attempts */
    u64 lockout_duration;         /* Lockout Duration */
    bool require_password_on_boot; /* Require Password on Boot */
    bool enable_keyring;          /* Enable Kernel Keyring */
    bool enable_trim;             /* Enable TRIM Support */
    
    /* Statistics */
    u64 total_encryptions;        /* Total Encryptions */
    u64 total_decryptions;        /* Total Decryptions */
    u64 total_failed;             /* Total Failed */
    
    /* Callbacks */
    int (*on_device_added)(encrypted_device_t *);
    int (*on_device_removed)(encrypted_device_t *);
    int (*on_unlock_attempt)(encrypted_device_t *, bool);
    
    /* User Data */
    void *user_data;
} encryption_manager_t;

/*===========================================================================*/
/*                         FUNCTION DECLARATIONS                             */
/*===========================================================================*/

/* Manager lifecycle */
int encryption_manager_init(encryption_manager_t *manager);
int encryption_manager_shutdown(encryption_manager_t *manager);
bool encryption_manager_is_initialized(encryption_manager_t *manager);

/* Device operations */
int encryption_manager_add_device(encryption_manager_t *manager, encrypted_device_t *dev);
int encryption_manager_remove_device(encryption_manager_t *manager, u32 device_id);
encrypted_device_t *encryption_manager_get_device(encryption_manager_t *manager, u32 device_id);
int encryption_manager_list_devices(encryption_manager_t *manager, encrypted_device_t *devices, u32 *count);

/* LUKS operations */
int luks_format_device(encrypted_device_t *dev, const char *passphrase, u32 cipher, u32 mode, u32 hash, u32 key_size);
int luks_open_device(encrypted_device_t *dev, const char *passphrase);
int luks_close_device(encrypted_device_t *dev);
int luks_change_key(encrypted_device_t *dev, u32 key_slot, const char *old_passphrase, const char *new_passphrase);
int luks_add_key(encrypted_device_t *dev, const char *passphrase, const char *new_passphrase);
int luks_remove_key(encrypted_device_t *dev, u32 key_slot, const char *passphrase);
int luks_suspend_device(encrypted_device_t *dev);
int luks_resume_device(encrypted_device_t *dev, const char *passphrase);

/* Key operations */
int encryption_create_key(encryption_manager_t *manager, u32 device_id, const char *passphrase, u32 *key_slot);
int encryption_delete_key(encryption_manager_t *manager, u32 device_id, u32 key_slot);
int encryption_verify_key(encryption_manager_t *manager, u32 device_id, const char *passphrase);
int encryption_get_key_info(encryption_manager_t *manager, u32 device_id, u32 key_slot, encryption_key_t *key);

/* Encryption/Decryption */
int encryption_encrypt_data(encrypted_device_t *dev, u64 sector, u32 count, void *buffer);
int encryption_decrypt_data(encrypted_device_t *dev, u64 sector, u32 count, void *buffer);
int encryption_encrypt_buffer(const u8 *key, u32 key_size, const void *in, void *out, u32 size);
int encryption_decrypt_buffer(const u8 *key, u32 key_size, const void *in, void *out, u32 size);

/* Key derivation */
int derive_key_pbkdf2(const char *passphrase, const u8 *salt, u32 salt_size, u32 iterations, u8 *key, u32 key_size);
int derive_key_scrypt(const char *passphrase, const u8 *salt, u32 salt_size, u32 n, u32 r, u32 p, u8 *key, u32 key_size);
int derive_key_argon2(const char *passphrase, const u8 *salt, u32 salt_size, u32 iterations, u32 memory, u32 parallelism, u8 *key, u32 key_size);

/* Hardware crypto */
bool encryption_has_hardware_crypto(void);
int encryption_init_hardware_crypto(void);
int encryption_cleanup_hardware_crypto(void);

/* Utility functions */
const char *encryption_get_cipher_name(u32 cipher);
const char *encryption_get_mode_name(u32 mode);
const char *encryption_get_hash_name(u32 hash);
const char *encryption_get_status_name(u32 status);
u32 encryption_get_key_size_bits(u32 cipher);
int encryption_generate_salt(u8 *salt, u32 size);
int encryption_generate_uuid(char *uuid, u32 size);

/* Global instance */
encryption_manager_t *encryption_manager_get_instance(void);

#endif /* _ENCRYPTION_MANAGER_H */
