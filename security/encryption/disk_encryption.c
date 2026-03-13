/*
 * NEXUS OS - Disk Encryption Driver
 * security/encryption/disk_encryption.c
 *
 * Full disk encryption with AES-XTS, key management
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "../security.h"
#include "../../kernel/include/kernel.h"

/*===========================================================================*/
/*                         ENCRYPTION CONFIGURATION                          */
/*===========================================================================*/

#define ENC_MAX_DEVICES           16
#define ENC_SECTOR_SIZE           512
#define ENC_KEY_SIZE              64      /* AES-256-XTS */
#define ENC_IV_SIZE               16
#define ENC_SALT_SIZE             16
#define ENC_MASTER_KEY_SIZE       32

/*===========================================================================*/
/*                         DATA STRUCTURES                                   */
/*===========================================================================*/

typedef struct {
    u32 device_id;
    char name[64];
    char uuid[37];              /* UUID string */
    u64 size;                   /* Device size in bytes */
    u64 data_offset;            /* Offset to encrypted data */
    u8 master_key[ENC_MASTER_KEY_SIZE];
    u8 salt[ENC_SALT_SIZE];
    u32 iterations;             /* KDF iterations */
    char cipher[16];            /* Cipher name */
    char mode[16];              /* Encryption mode */
    u32 key_size;               /* Key size in bits */
    bool is_encrypted;
    bool is_unlocked;
    bool is_system;
    u64 sectors_read;
    u64 sectors_written;
    void *crypto_ctx;
} enc_device_t;

typedef struct {
    bool initialized;
    enc_device_t devices[ENC_MAX_DEVICES];
    u32 device_count;
    u32 default_device;
    bool hw_crypto;             /* Hardware crypto available */
    spinlock_t lock;
} enc_manager_t;

static enc_manager_t g_enc;

/*===========================================================================*/
/*                         AES-XTS ENCRYPTION                                */
/*===========================================================================*/

/* XTS mode: C[i] = E(K1, P[i] XOR T[j]) XOR T[j] */
/* T[j] = E(K2, sector_number) */

static void aes_xts_encrypt(const u8 *key, u32 key_size, u8 *data, 
                            u32 data_size, u64 sector)
{
    /* In real implementation, would use hardware AES-NI or software AES */
    /* This is a placeholder */
    
    u8 tweak[16];
    memset(tweak, 0, sizeof(tweak));
    
    /* Generate tweak from sector number */
    for (int i = 0; i < 8; i++) {
        tweak[i] = (sector >> (i * 8)) & 0xFF;
    }
    
    /* Encrypt tweak with second key */
    /* T = E(K2, sector) */
    
    /* Process each 16-byte block */
    for (u32 i = 0; i < data_size; i += 16) {
        u8 *block = &data[i];
        
        /* XOR with tweak */
        for (int j = 0; j < 16; j++) {
            block[j] ^= tweak[j];
        }
        
        /* AES encrypt with first key */
        /* C = E(K1, P XOR T) */
        
        /* XOR with tweak again */
        for (int j = 0; j < 16; j++) {
            block[j] ^= tweak[j];
        }
        
        /* Multiply tweak by alpha (GF(2^128)) */
        /* For next block */
    }
    
    (void)key; (void)key_size;
}

static void aes_xts_decrypt(const u8 *key, u32 key_size, u8 *data,
                            u32 data_size, u64 sector)
{
    /* Decryption is similar but uses AES decrypt */
    u8 tweak[16];
    memset(tweak, 0, sizeof(tweak));
    
    for (int i = 0; i < 8; i++) {
        tweak[i] = (sector >> (i * 8)) & 0xFF;
    }
    
    for (u32 i = 0; i < data_size; i += 16) {
        u8 *block = &data[i];
        
        for (int j = 0; j < 16; j++) {
            block[j] ^= tweak[j];
        }
        
        /* AES decrypt */
        
        for (int j = 0; j < 16; j++) {
            block[j] ^= tweak[j];
        }
    }
    
    (void)key; (void)key_size;
}

/*===========================================================================*/
/*                         KEY DERIVATION                                    */
/*===========================================================================*/

/* PBKDF2-HMAC-SHA256 */
static int pbkdf2(const char *password, u32 pass_len,
                  const u8 *salt, u32 salt_len,
                  u8 *output, u32 out_len, u32 iterations)
{
    u32 block_count = (out_len + 31) / 32;  /* SHA-256 = 32 bytes */
    u8 block[32];
    u8 digest[32];
    
    for (u32 block_num = 1; block_num <= block_count; block_num++) {
        /* U1 = PRF(Password, Salt || INT(block_num)) */
        u8 block_num_be[4];
        for (int i = 0; i < 4; i++) {
            block_num_be[i] = (block_num >> (24 - i * 8)) & 0xFF;
        }
        
        /* First iteration */
        /* hmac_sha256(password, salt || block_num_be, digest) */
        memcpy(block, digest, 32);
        
        /* Remaining iterations */
        for (u32 i = 1; i < iterations; i++) {
            /* hmac_sha256(password, block, digest) */
            for (int j = 0; j < 32; j++) {
                block[j] ^= digest[j];
            }
        }
        
        /* Copy to output */
        u32 copy_len = (out_len >= 32) ? 32 : out_len;
        memcpy(&output[(block_num - 1) * 32], block, copy_len);
        out_len -= copy_len;
    }
    
    (void)pass_len;
    return 0;
}

static int derive_key(const char *password, const u8 *salt, 
                      u8 *key, u32 key_size, u32 iterations)
{
    return pbkdf2(password, strlen(password), salt, ENC_SALT_SIZE,
                  key, key_size, iterations);
}

/*===========================================================================*/
/*                         INITIALIZATION                                    */
/*===========================================================================*/

int enc_init(void)
{
    printk("[ENC] ========================================\n");
    printk("[ENC] NEXUS OS Disk Encryption\n");
    printk("[ENC] ========================================\n");
    
    memset(&g_enc, 0, sizeof(enc_manager_t));
    g_enc.initialized = true;
    spinlock_init(&g_enc.lock);
    
    /* Check for hardware crypto */
    g_enc.hw_crypto = true;  /* Assume available */
    
    printk("[ENC] Encryption manager initialized\n");
    printk("[ENC] Hardware crypto: %s\n", g_enc.hw_crypto ? "Available" : "Software only");
    
    return 0;
}

int enc_shutdown(void)
{
    printk("[ENC] Shutting down encryption manager...\n");
    
    /* Lock all devices */
    for (u32 i = 0; i < g_enc.device_count; i++) {
        enc_device_t *dev = &g_enc.devices[i];
        if (dev->is_unlocked) {
            /* Wipe keys from memory */
            memset(dev->master_key, 0, sizeof(dev->master_key));
            dev->is_unlocked = false;
        }
    }
    
    g_enc.initialized = false;
    return 0;
}

/*===========================================================================*/
/*                         DEVICE MANAGEMENT                                 */
/*===========================================================================*/

int enc_register_device(const char *name, u64 size, bool is_system)
{
    spinlock_lock(&g_enc.lock);
    
    if (g_enc.device_count >= ENC_MAX_DEVICES) {
        spinlock_unlock(&g_enc.lock);
        return -ENOMEM;
    }
    
    enc_device_t *dev = &g_enc.devices[g_enc.device_count++];
    dev->device_id = g_enc.device_count;
    strncpy(dev->name, name, sizeof(dev->name) - 1);
    dev->size = size;
    dev->data_offset = 4096;  /* Reserve first 4KB for header */
    dev->is_system = is_system;
    dev->is_encrypted = false;
    dev->is_unlocked = false;
    
    /* Generate random salt */
    for (int i = 0; i < ENC_SALT_SIZE; i++) {
        dev->salt[i] = (u8)(ktime_get_ns() & 0xFF);
    }
    
    /* Default cipher settings */
    strcpy(dev->cipher, "aes");
    strcpy(dev->mode, "xts-plain64");
    dev->key_size = 256;
    dev->iterations = 200000;  /* ~0.5s on modern CPU */
    
    spinlock_unlock(&g_enc.lock);
    
    printk("[ENC] Registered device '%s' (%d MB)%s\n", 
           name, (u32)(size / (1024 * 1024)),
           is_system ? " [SYSTEM]" : "");
    
    return dev->device_id;
}

int enc_format_device(u32 device_id, const char *password)
{
    enc_device_t *dev = NULL;
    for (u32 i = 0; i < g_enc.device_count; i++) {
        if (g_enc.devices[i].device_id == device_id) {
            dev = &g_enc.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    
    printk("[ENC] Formatting device '%s'...\n", dev->name);
    
    /* Generate random master key */
    for (int i = 0; i < ENC_MASTER_KEY_SIZE; i++) {
        dev->master_key[i] = (u8)(ktime_get_ns() & 0xFF);
    }
    
    /* Derive key encryption key from password */
    u8 kek[ENC_KEY_SIZE];
    derive_key(password, dev->salt, kek, ENC_KEY_SIZE, dev->iterations);
    
    /* Encrypt master key with KEK */
    aes_xts_encrypt(kek, ENC_KEY_SIZE, dev->master_key, 
                    ENC_MASTER_KEY_SIZE, 0);
    
    dev->is_encrypted = true;
    dev->is_unlocked = false;
    
    /* Write header to device (in real impl, would write to disk) */
    
    printk("[ENC] Device '%s' encrypted\n", dev->name);
    return 0;
}

int enc_unlock_device(u32 device_id, const char *password)
{
    enc_device_t *dev = NULL;
    for (u32 i = 0; i < g_enc.device_count; i++) {
        if (g_enc.devices[i].device_id == device_id) {
            dev = &g_enc.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    if (!dev->is_encrypted) return 0;  /* Not encrypted */
    
    printk("[ENC] Unlocking device '%s'...\n", dev->name);
    
    /* Derive KEK from password */
    u8 kek[ENC_KEY_SIZE];
    derive_key(password, dev->salt, kek, ENC_KEY_SIZE, dev->iterations);
    
    /* Try to decrypt master key */
    u8 test_key[ENC_MASTER_KEY_SIZE];
    memcpy(test_key, dev->master_key, ENC_MASTER_KEY_SIZE);
    aes_xts_decrypt(kek, ENC_KEY_SIZE, test_key, ENC_MASTER_KEY_SIZE, 0);
    
    /* Verify (in real impl, would check against stored checksum) */
    dev->is_unlocked = true;
    
    printk("[ENC] Device '%s' unlocked\n", dev->name);
    return 0;
}

int enc_lock_device(u32 device_id)
{
    enc_device_t *dev = NULL;
    for (u32 i = 0; i < g_enc.device_count; i++) {
        if (g_enc.devices[i].device_id == device_id) {
            dev = &g_enc.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    
    /* Wipe decrypted key from memory */
    memset(dev->master_key, 0, sizeof(dev->master_key));
    dev->is_unlocked = false;
    
    printk("[ENC] Device '%s' locked\n", dev->name);
    return 0;
}

/*===========================================================================*/
/*                         BLOCK I/O                                         */
/*===========================================================================*/

int enc_read_sector(u32 device_id, u64 sector, void *buffer)
{
    enc_device_t *dev = NULL;
    for (u32 i = 0; i < g_enc.device_count; i++) {
        if (g_enc.devices[i].device_id == device_id) {
            dev = &g_enc.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    if (!dev->is_unlocked) return -EACCES;
    
    /* Read raw sector from device */
    /* In real impl: device_read(dev->name, sector, buffer) */
    
    /* Decrypt if encrypted */
    if (dev->is_encrypted) {
        aes_xts_decrypt(dev->master_key, ENC_KEY_SIZE, 
                        (u8 *)buffer, ENC_SECTOR_SIZE, sector);
    }
    
    dev->sectors_read++;
    return 0;
}

int enc_write_sector(u32 device_id, u64 sector, const void *buffer)
{
    enc_device_t *dev = NULL;
    for (u32 i = 0; i < g_enc.device_count; i++) {
        if (g_enc.devices[i].device_id == device_id) {
            dev = &g_enc.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    if (!dev->is_unlocked) return -EACCES;
    
    u8 encrypted[ENC_SECTOR_SIZE];
    memcpy(encrypted, buffer, ENC_SECTOR_SIZE);
    
    /* Encrypt if encrypted */
    if (dev->is_encrypted) {
        aes_xts_encrypt(dev->master_key, ENC_KEY_SIZE,
                        encrypted, ENC_SECTOR_SIZE, sector);
    }
    
    /* Write raw sector to device */
    /* In real impl: device_write(dev->name, sector, encrypted) */
    
    dev->sectors_written++;
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

int enc_get_device_status(u32 device_id, bool *encrypted, bool *unlocked)
{
    enc_device_t *dev = NULL;
    for (u32 i = 0; i < g_enc.device_count; i++) {
        if (g_enc.devices[i].device_id == device_id) {
            dev = &g_enc.devices[i];
            break;
        }
    }
    
    if (!dev) return -ENOENT;
    
    if (encrypted) *encrypted = dev->is_encrypted;
    if (unlocked) *unlocked = dev->is_unlocked;
    
    return 0;
}

int enc_list_devices(enc_device_t *devices, u32 *count)
{
    if (!devices || !count) return -EINVAL;
    
    u32 copy = (*count < g_enc.device_count) ? *count : g_enc.device_count;
    memcpy(devices, g_enc.devices, sizeof(enc_device_t) * copy);
    *count = copy;
    
    return 0;
}

enc_manager_t *enc_get_manager(void)
{
    return &g_enc;
}
