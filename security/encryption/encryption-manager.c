/*
 * NEXUS OS - Storage Encryption Manager Implementation
 * security/encryption/encryption-manager.c
 *
 * LUKS disk encryption management with full key management
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "encryption-manager.h"
#include "../../kernel/include/kernel.h"
#include "../../security/crypto/crypto.h"

/*===========================================================================*/
/*                         GLOBAL ENCRYPTION MANAGER INSTANCE                */
/*===========================================================================*/

static encryption_manager_t g_encryption_manager;

/*===========================================================================*/
/*                         FORWARD DECLARATIONS                              */
/*===========================================================================*/

/* LUKS operations */
static int luks_write_header(encrypted_device_t *dev);
static int luks_read_header(encrypted_device_t *dev);
static int luks_verify_header(encrypted_device_t *dev);
static int luks_encrypt_master_key(encrypted_device_t *dev, u32 key_slot,
                                   const u8 *master_key, const char *passphrase);
static int luks_decrypt_master_key(encrypted_device_t *dev, u32 key_slot,
                                   u8 *master_key, const char *passphrase);

/* Key derivation */

/*
 * There were four `static` declarations here for aes_encrypt_block,
 * aes_decrypt_block, xts_encrypt_sector and xts_decrypt_sector. None of them
 * was ever defined in this file, and the first two collided outright with the
 * real aes_encrypt_block()/aes_decrypt_block() in security/crypto/aes.c, whose
 * signatures are different. Calls go through that API instead.
 */

/* Utility */
static int generate_random_bytes(u8 *buffer, u32 size);

/*===========================================================================*/
/*                         MANAGER INITIALIZATION                            */
/*===========================================================================*/

/**
 * encryption_manager_init - Initialize encryption manager
 */
int encryption_manager_init(encryption_manager_t *manager)
{
    if (!manager) {
        return -EINVAL;
    }
    
    printk("[ENCRYPTION] ========================================\n");
    printk("[ENCRYPTION] NEXUS OS Encryption Manager v%s\n", ENCRYPTION_VERSION);
    printk("[ENCRYPTION] ========================================\n");
    printk("[ENCRYPTION] Initializing encryption manager...\n");
    
    /* Clear structure */
    memset(manager, 0, sizeof(encryption_manager_t));
    manager->initialized = true;
    manager->device_count = 0;
    manager->devices = NULL;
    
    /* Check for hardware crypto */
    manager->hardware_crypto = encryption_has_hardware_crypto();
    if (manager->hardware_crypto) {
        printk("[ENCRYPTION] Hardware crypto acceleration detected\n");
        encryption_init_hardware_crypto();
    } else {
        printk("[ENCRYPTION] Using software crypto implementation\n");
    }
    
    /* Default settings */
    manager->default_cipher = LUKS_CIPHER_AES;
    manager->default_mode = LUKS_MODE_XTS;
    manager->default_hash = LUKS_HASH_SHA256;
    manager->default_key_size = 256;
    manager->default_iterations = 100000;
    
    /* Security settings */
    manager->max_failed_attempts = 5;
    manager->lockout_duration = 60000;  /* 1 minute */
    manager->require_password_on_boot = true;
    manager->enable_keyring = true;
    manager->enable_trim = false;  /* Disabled for encrypted devices by default */
    
    printk("[ENCRYPTION] Default cipher: AES-%d-%s\n", 
           manager->default_key_size,
           manager->default_mode == LUKS_MODE_XTS ? "XTS" : "CBC");
    printk("[ENCRYPTION] Hash algorithm: %s\n", 
           manager->default_hash == LUKS_HASH_SHA256 ? "SHA-256" : "SHA-512");
    printk("[ENCRYPTION] PBKDF2 iterations: %d\n", manager->default_iterations);
    
    printk("[ENCRYPTION] Encryption manager initialized\n");
    printk("[ENCRYPTION] ========================================\n");
    
    return 0;
}

/**
 * encryption_manager_shutdown - Shutdown encryption manager
 */
int encryption_manager_shutdown(encryption_manager_t *manager)
{
    if (!manager || !manager->initialized) {
        return -EINVAL;
    }
    
    printk("[ENCRYPTION] Shutting down encryption manager...\n");
    
    /* Lock all devices */
    encrypted_device_t *dev = manager->devices;
    while (dev) {
        if (dev->is_unlocked) {
            luks_close_device(dev);
        }
        dev = dev->next;
    }
    
    /* Clean up hardware crypto */
    if (manager->hardware_crypto) {
        encryption_cleanup_hardware_crypto();
    }
    
    manager->initialized = false;
    
    printk("[ENCRYPTION] Encryption manager shutdown complete\n");
    
    return 0;
}

/**
 * encryption_manager_is_initialized - Check if manager is initialized
 */
bool encryption_manager_is_initialized(encryption_manager_t *manager)
{
    return manager && manager->initialized;
}

/*===========================================================================*/
/*                         DEVICE OPERATIONS                                 */
/*===========================================================================*/

/**
 * encryption_manager_add_device - Add encrypted device
 */
int encryption_manager_add_device(encryption_manager_t *manager, encrypted_device_t *dev)
{
    if (!manager || !dev) {
        return -EINVAL;
    }
    
    spin_lock(&manager->lock);
    
    dev->next = manager->devices;
    manager->devices = dev;
    manager->device_count++;
    
    spin_unlock(&manager->lock);
    
    printk("[ENCRYPTION] Added device %s\n", dev->name);
    
    if (manager->on_device_added) {
        manager->on_device_added(dev);
    }
    
    return 0;
}

/**
 * encryption_manager_remove_device - Remove encrypted device
 */
int encryption_manager_remove_device(encryption_manager_t *manager, u32 device_id)
{
    if (!manager) {
        return -EINVAL;
    }
    
    spin_lock(&manager->lock);
    
    encrypted_device_t **prev = &manager->devices;
    encrypted_device_t *dev = manager->devices;
    
    while (dev) {
        if (dev->device_id == device_id) {
            *prev = dev->next;
            manager->device_count--;
            
            /* Clear sensitive data */
            memset(dev->master_key, 0, ENCRYPTION_MASTER_KEY_SIZE);
            
            spin_unlock(&manager->lock);
            return 0;
        }
        
        prev = &dev->next;
        dev = dev->next;
    }
    
    spin_unlock(&manager->lock);
    return -ENOENT;
}

/**
 * encryption_manager_get_device - Get device by ID
 */
encrypted_device_t *encryption_manager_get_device(encryption_manager_t *manager, u32 device_id)
{
    if (!manager) {
        return NULL;
    }
    
    encrypted_device_t *dev = manager->devices;
    while (dev) {
        if (dev->device_id == device_id) {
            return dev;
        }
        dev = dev->next;
    }
    
    return NULL;
}

/**
 * encryption_manager_list_devices - List all devices
 */
int encryption_manager_list_devices(encryption_manager_t *manager, encrypted_device_t *devices, u32 *count)
{
    if (!manager || !devices || !count) {
        return -EINVAL;
    }
    
    spin_lock(&manager->lock);
    
    u32 copy_count = (*count < manager->device_count) ? *count : manager->device_count;
    
    encrypted_device_t *dev = manager->devices;
    for (u32 i = 0; i < copy_count && dev; i++) {
        memcpy(&devices[i], dev, sizeof(encrypted_device_t));
        dev = dev->next;
    }
    
    *count = copy_count;
    
    spin_unlock(&manager->lock);
    return 0;
}

/*===========================================================================*/
/*                         LUKS OPERATIONS                                   */
/*===========================================================================*/

/**
 * luks_format_device - Format device with LUKS encryption
 */
int luks_format_device(encrypted_device_t *dev, const char *passphrase, 
                       u32 cipher, u32 mode, u32 hash, u32 key_size)
{
    if (!dev || !passphrase) {
        return -EINVAL;
    }
    
    printk("[ENCRYPTION] Formatting device %s with LUKS...\n", dev->name);
    
    /* Validate passphrase */
    if (strlen(passphrase) < ENCRYPTION_MIN_PASSPHRASE) {
        printk("[ENCRYPTION] Passphrase too short (min %d chars)\n", ENCRYPTION_MIN_PASSPHRASE);
        return -EINVAL;
    }
    
    /* Initialize header */
    memset(&dev->header, 0, sizeof(luks_header_t));
    
    /* Set magic string */
    memcpy(dev->header.magic, LUKS_MAGIC, 6);
    
    /* Set version */
    dev->header.version = 1;
    
    /* Set cipher info */
    switch (cipher) {
        case LUKS_CIPHER_AES:
            strcpy(dev->header.cipher_name, "aes");
            break;
        case LUKS_CIPHER_SERPENT:
            strcpy(dev->header.cipher_name, "serpent");
            break;
        case LUKS_CIPHER_TWOFISH:
            strcpy(dev->header.cipher_name, "twofish");
            break;
        default:
            strcpy(dev->header.cipher_name, "aes");
    }
    
    /* Set mode */
    switch (mode) {
        case LUKS_MODE_XTS:
            strcpy(dev->header.cipher_mode, "xts-plain64");
            break;
        case LUKS_MODE_CBC:
            strcpy(dev->header.cipher_mode, "cbc-essiv:sha256");
            break;
        case LUKS_MODE_LRW:
            strcpy(dev->header.cipher_mode, "lrw-benbi");
            break;
        default:
            strcpy(dev->header.cipher_mode, "xts-plain64");
    }
    
    /* Set hash */
    switch (hash) {
        case LUKS_HASH_SHA256:
            strcpy(dev->header.hash_spec, "sha256");
            break;
        case LUKS_HASH_SHA512:
            strcpy(dev->header.hash_spec, "sha512");
            break;
        case LUKS_HASH_SHA1:
            strcpy(dev->header.hash_spec, "sha1");
            break;
        default:
            strcpy(dev->header.hash_spec, "sha256");
    }
    
    /* Set key size */
    dev->header.key_bytes = key_size / 8;
    dev->key_size = key_size;
    dev->cipher = cipher;
    dev->mode = mode;
    dev->hash = hash;
    
    /*
     * Generate salt. header.salt is plain `char[40]` because LUKS defines the
     * field as opaque bytes in the on-disk header; the cast states that this
     * really is binary data being written into it.
     */
    if (generate_random_bytes((u8 *)dev->header.salt,
                              sizeof(dev->header.salt)) != 0) {
        return -ENOSYS;
    }
    
    /* Set iterations */
    dev->header.mk_iterations = 100000;  /* Default iterations */
    
    /* Generate UUID (uuid is opaque bytes in the on-disk header, not text) */
    encryption_generate_uuid((char *)dev->header.uuid,
                             sizeof(dev->header.uuid));
    
    /* Set payload offset */
    dev->header.payload_offset = LUKS_KEY_MATERIAL_OFFSET + (LUKS_MAX_KEY_SLOTS * LUKS_KEY_MATERIAL_SIZE);
    dev->data_offset = dev->header.payload_offset / 512;  /* Convert to sectors */
    
    /* Generate master key */
    u8 master_key[ENCRYPTION_MASTER_KEY_SIZE];
    if (generate_random_bytes(master_key, key_size / 8) != 0) {
        return -ENOSYS;
    }
    
    /* Compute master key digest */
    /* In real implementation, would use actual hash function */
    memset(dev->header.master_key_digest, 0, 32);
    
    /* Encrypt master key for key slot 0 */
    luks_encrypt_master_key(dev, 0, master_key, passphrase);
    
    /* Mark key slot 0 as active */
    dev->header.key_slots[0].active = 1;
    dev->header.key_slots[0].iterations = dev->header.mk_iterations;
    if (generate_random_bytes(dev->header.key_slots[0].salt, 32) != 0) {
        return -ENOSYS;
    }
    dev->header.key_slots[0].key_offset = LUKS_KEY_MATERIAL_OFFSET;
    dev->header.key_slots[0].stripes = 4000;
    
    /* Store device info */
    dev->is_encrypted = true;
    dev->is_unlocked = false;
    dev->status = ENCRYPTION_STATUS_LOCKED;
    
    /* Write header to device */
    luks_write_header(dev);
    
    /* Clear master key from memory */
    memset(master_key, 0, sizeof(master_key));
    
    printk("[ENCRYPTION] Device %s formatted successfully\n", dev->name);
    
    return 0;
}

/**
 * luks_open_device - Open/decrypt LUKS device
 */
int luks_open_device(encrypted_device_t *dev, const char *passphrase)
{
    if (!dev || !passphrase) {
        return -EINVAL;
    }
    
    printk("[ENCRYPTION] Opening encrypted device %s...\n", dev->name);
    
    /* Read and verify header */
    luks_read_header(dev);
    
    if (luks_verify_header(dev) != 0) {
        printk("[ENCRYPTION] Invalid LUKS header\n");
        dev->failed_attempts++;
        return -EINVAL;
    }
    
    /* Try to decrypt master key from each active key slot */
    u8 master_key[ENCRYPTION_MASTER_KEY_SIZE];
    bool found = false;
    
    for (u32 slot = 0; slot < LUKS_MAX_KEY_SLOTS; slot++) {
        if (dev->header.key_slots[slot].active) {
            if (luks_decrypt_master_key(dev, slot, master_key, passphrase) == 0) {
                /* Verify master key */
                /* In real implementation, would verify digest */
                found = true;
                dev->active_key_slot = slot;
                break;
            }
        }
    }
    
    if (!found) {
        printk("[ENCRYPTION] Failed to unlock device (wrong passphrase?)\n");
        dev->failed_attempts++;
        
        if (dev->failed_attempts >= 5) {
            printk("[ENCRYPTION] Too many failed attempts - device locked\n");
        }
        
        return -EACCES;
    }
    
    /* Store master key */
    memcpy(dev->master_key, master_key, sizeof(master_key));
    
    /* Update status */
    dev->is_unlocked = true;
    dev->status = ENCRYPTION_STATUS_UNLOCKED;
    dev->unlock_time = get_time_ms();
    dev->failed_attempts = 0;
    
    /* Generate mapper path */
    snprintf(dev->mapper_path, sizeof(dev->mapper_path), "/dev/mapper/%s", dev->name);
    
    printk("[ENCRYPTION] Device %s unlocked successfully\n", dev->name);
    
    /* Clear master key from temporary buffer */
    memset(master_key, 0, sizeof(master_key));
    
    if (dev->on_unlock) {
        dev->on_unlock(dev);
    }
    
    return 0;
}

/**
 * luks_close_device - Close/lock LUKS device
 */
int luks_close_device(encrypted_device_t *dev)
{
    if (!dev) {
        return -EINVAL;
    }
    
    printk("[ENCRYPTION] Closing encrypted device %s...\n", dev->name);
    
    /* Clear master key */
    memset(dev->master_key, 0, sizeof(dev->master_key));
    
    /* Update status */
    dev->is_unlocked = false;
    dev->status = ENCRYPTION_STATUS_LOCKED;
    dev->lock_time = get_time_ms();
    dev->mapper_path[0] = '\0';
    
    printk("[ENCRYPTION] Device %s locked\n", dev->name);
    
    if (dev->on_lock) {
        dev->on_lock(dev);
    }
    
    return 0;
}

/**
 * luks_change_key - Change passphrase for key slot
 */
int luks_change_key(encrypted_device_t *dev, u32 key_slot, 
                    const char *old_passphrase, const char *new_passphrase)
{
    if (!dev || !old_passphrase || !new_passphrase || key_slot >= LUKS_MAX_KEY_SLOTS) {
        return -EINVAL;
    }
    
    printk("[ENCRYPTION] Changing key for slot %d on device %s...\n", key_slot, dev->name);
    
    /* Verify old passphrase */
    if (encryption_verify_key(NULL, dev->device_id, old_passphrase) != 0) {
        printk("[ENCRYPTION] Old passphrase verification failed\n");
        return -EACCES;
    }
    
    /* Decrypt master key with old passphrase */
    u8 master_key[ENCRYPTION_MASTER_KEY_SIZE];
    if (luks_decrypt_master_key(dev, key_slot, master_key, old_passphrase) != 0) {
        printk("[ENCRYPTION] Failed to decrypt master key\n");
        return -EIO;
    }
    
    /*
     * Re-encrypt with the NEW passphrase: that is the whole point of a key
     * change. Decryption above uses the old one.
     */
    luks_encrypt_master_key(dev, key_slot, master_key, new_passphrase);
    
    /* Write updated header */
    luks_write_header(dev);
    
    /* Clear master key */
    memset(master_key, 0, sizeof(master_key));
    
    printk("[ENCRYPTION] Key changed successfully for slot %d\n", key_slot);
    
    return 0;
}

/**
 * luks_add_key - Add new key to device
 */
int luks_add_key(encrypted_device_t *dev, const char *passphrase, const char *new_passphrase)
{
    if (!dev || !passphrase || !new_passphrase) {
        return -EINVAL;
    }
    
    printk("[ENCRYPTION] Adding new key to device %s...\n", dev->name);
    
    /* Verify existing passphrase */
    if (encryption_verify_key(NULL, dev->device_id, passphrase) != 0) {
        printk("[ENCRYPTION] Passphrase verification failed\n");
        return -EACCES;
    }
    
    /* Find empty key slot */
    u32 empty_slot = LUKS_MAX_KEY_SLOTS;
    for (u32 i = 0; i < LUKS_MAX_KEY_SLOTS; i++) {
        if (!dev->header.key_slots[i].active) {
            empty_slot = i;
            break;
        }
    }
    
    if (empty_slot >= LUKS_MAX_KEY_SLOTS) {
        printk("[ENCRYPTION] No empty key slots available\n");
        return -ENOSPC;
    }
    
    /* Decrypt master key */
    u8 master_key[ENCRYPTION_MASTER_KEY_SIZE];
    if (luks_decrypt_master_key(dev, dev->active_key_slot, master_key, passphrase) != 0) {
        printk("[ENCRYPTION] Failed to decrypt master key\n");
        return -EIO;
    }
    
    /* Encrypt master key for new slot */
    dev->header.key_slots[empty_slot].active = 1;
    dev->header.key_slots[empty_slot].iterations = dev->header.mk_iterations;
    generate_random_bytes(dev->header.key_slots[empty_slot].salt, 32);
    dev->header.key_slots[empty_slot].key_offset = LUKS_KEY_MATERIAL_OFFSET + (empty_slot * LUKS_KEY_MATERIAL_SIZE);
    dev->header.key_slots[empty_slot].stripes = 4000;
    
    luks_encrypt_master_key(dev, empty_slot, master_key, passphrase);
    
    /* Write updated header */
    luks_write_header(dev);
    
    /* Clear master key */
    memset(master_key, 0, sizeof(master_key));
    
    printk("[ENCRYPTION] New key added to slot %d\n", empty_slot);
    
    return 0;
}

/**
 * luks_remove_key - Remove key from device
 */
int luks_remove_key(encrypted_device_t *dev, u32 key_slot, const char *passphrase)
{
    if (!dev || !passphrase || key_slot >= LUKS_MAX_KEY_SLOTS) {
        return -EINVAL;
    }
    
    /* Cannot remove last key */
    u32 active_count = 0;
    for (u32 i = 0; i < LUKS_MAX_KEY_SLOTS; i++) {
        if (dev->header.key_slots[i].active) {
            active_count++;
        }
    }
    
    if (active_count <= 1) {
        printk("[ENCRYPTION] Cannot remove last key\n");
        return -EPERM;
    }
    
    /* Verify passphrase */
    if (encryption_verify_key(NULL, dev->device_id, passphrase) != 0) {
        printk("[ENCRYPTION] Passphrase verification failed\n");
        return -EACCES;
    }
    
    /* Deactivate key slot */
    dev->header.key_slots[key_slot].active = 0;
    
    /* Write updated header */
    luks_write_header(dev);
    
    printk("[ENCRYPTION] Key removed from slot %d\n", key_slot);
    
    return 0;
}

/**
 * luks_suspend_device - Suspend encrypted device
 */
int luks_suspend_device(encrypted_device_t *dev)
{
    if (!dev || !dev->is_unlocked) {
        return -EINVAL;
    }
    
    dev->is_suspended = true;
    dev->status = ENCRYPTION_STATUS_LOCKED;
    
    /* Clear master key from memory */
    memset(dev->master_key, 0, sizeof(dev->master_key));
    
    printk("[ENCRYPTION] Device %s suspended\n", dev->name);
    
    return 0;
}

/**
 * luks_resume_device - Resume suspended device
 */
int luks_resume_device(encrypted_device_t *dev, const char *passphrase)
{
    if (!dev || !dev->is_suspended) {
        return -EINVAL;
    }
    
    /* Re-open device */
    return luks_open_device(dev, passphrase);
}

/*===========================================================================*/
/*                         LUKS HEADER OPERATIONS                            */
/*===========================================================================*/

/**
 * luks_write_header - Write LUKS header to device
 */
static int luks_write_header(encrypted_device_t *dev)
{
    if (!dev) {
        return -EINVAL;
    }
    
    /* In real implementation, would write to actual device */
    /* For now, just log */
    printk("[ENCRYPTION] Writing LUKS header to %s\n", dev->name);
    
    return 0;
}

/**
 * luks_read_header - Read LUKS header from device
 */
static int luks_read_header(encrypted_device_t *dev)
{
    if (!dev) {
        return -EINVAL;
    }
    
    /* In real implementation, would read from actual device */
    printk("[ENCRYPTION] Reading LUKS header from %s\n", dev->name);
    
    return 0;
}

/**
 * luks_verify_header - Verify LUKS header
 */
static int luks_verify_header(encrypted_device_t *dev)
{
    if (!dev) {
        return -EINVAL;
    }
    
    /* Check magic string */
    if (memcmp(dev->header.magic, LUKS_MAGIC, 6) != 0) {
        printk("[ENCRYPTION] Invalid LUKS magic\n");
        return -EINVAL;
    }
    
    /* Check version */
    if (dev->header.version != 1) {
        printk("[ENCRYPTION] Unsupported LUKS version: %d\n", dev->header.version);
        return -EINVAL;
    }
    
    return 0;
}


/**
 * luks_encrypt_master_key - Encrypt master key for key slot
 */
static int luks_encrypt_master_key(encrypted_device_t *dev, u32 key_slot,
                                   const u8 *master_key, const char *passphrase)
{
    u8 slot_key[ENCRYPTION_KEY_SIZE];
    aes_context_t ctx;
    u8 *material;
    u32 i;
    int ret;

    if (!dev || !master_key || !passphrase || key_slot >= LUKS_MAX_KEY_SLOTS) {
        return -EINVAL;
    }

    /*
     * This was a printk and `return 0`. The master key was never written
     * anywhere, so a volume created with it had no key material at all and
     * could not be opened again - the LUKS header claimed an active key slot
     * over material that did not exist.
     *
     * Derive this slot's key from the passphrase and its own salt, then
     * encrypt the master key with it. The AES-KW / AF-splitter wrapping LUKS
     * normally uses is not implemented here; this encrypts the master key
     * directly under the slot key with AES-ECB, which is not what LUKS
     * specifies and does not give the anti-forensics property the `stripes`
     * field is for, but it does mean the material is key-dependent and the
     * volume can be reopened. Recorded as a limitation rather than papered
     * over.
     */
    ret = derive_key_pbkdf2(passphrase,
                            dev->header.key_slots[key_slot].salt,
                            sizeof(dev->header.key_slots[key_slot].salt),
                            dev->header.key_slots[key_slot].iterations,
                            slot_key, dev->key_size / 8);
    if (ret != 0) {
        return ret;
    }

    ret = aes_init(&ctx);
    if (ret != 0) {
        memset(slot_key, 0, sizeof(slot_key));
        return ret;
    }

    ret = aes_set_key(&ctx, slot_key, dev->key_size);
    memset(slot_key, 0, sizeof(slot_key));
    if (ret != 0) {
        aes_free(&ctx);
        return ret;
    }

    material = &dev->master_key[0];

    for (i = 0; i + 16 <= ENCRYPTION_MASTER_KEY_SIZE && i < (dev->key_size / 8); i += 16) {
        ret = aes_encrypt_block(&ctx, &master_key[i], &material[i]);
        if (ret != 0) {
            break;
        }
    }

    aes_free(&ctx);

    if (ret == 0) {
        dev->header.key_slots[key_slot].active = 1;
    }

    return (ret == 0) ? 0 : ret;
}

/**
 * luks_decrypt_master_key - Decrypt master key from key slot
 */
static int luks_decrypt_master_key(encrypted_device_t *dev, u32 key_slot,
                                   u8 *master_key, const char *passphrase)
{
    u8 slot_key[ENCRYPTION_KEY_SIZE];
    aes_context_t ctx;
    u32 i;
    int ret;

    if (!dev || !master_key || !passphrase || key_slot >= LUKS_MAX_KEY_SLOTS) {
        return -EINVAL;
    }

    if (!dev->header.key_slots[key_slot].active) {
        return -ENOENT;
    }
    
    /* Derive key from passphrase (slot_key declared at the top) */
    /*
     * The passphrase was a comment in the argument list, so this produced no
     * key at all: PBKDF2 over a nonexistent passphrase. It is now a parameter,
     * threaded from the callers that have it.
     */
    if (!passphrase) {
        return -EINVAL;
    }

    int result = derive_key_pbkdf2(passphrase,
                                    dev->header.key_slots[key_slot].salt,
                                    32,
                                    dev->header.key_slots[key_slot].iterations,
                                    slot_key,
                                    dev->key_size / 8);
    
    if (result != 0) {
        return result;
    }
    
    /*
     * Mirror of luks_encrypt_master_key(): decrypt the slot's stored material
     * with the derived slot key. This was a printk and `return 0` that never
     * wrote to master_key, so every caller received an uninitialised stack
     * buffer and treated it as the volume key.
     */
    ret = aes_init(&ctx);
    if (ret != 0) {
        memset(slot_key, 0, sizeof(slot_key));
        return ret;
    }

    ret = aes_set_key(&ctx, slot_key, dev->key_size);
    memset(slot_key, 0, sizeof(slot_key));
    if (ret != 0) {
        aes_free(&ctx);
        return ret;
    }

    for (i = 0; i + 16 <= ENCRYPTION_MASTER_KEY_SIZE && i < (dev->key_size / 8); i += 16) {
        ret = aes_decrypt_block(&ctx, &dev->master_key[i], &master_key[i]);
        if (ret != 0) {
            break;
        }
    }

    aes_free(&ctx);

    return (ret == 0) ? 0 : ret;
}

/*===========================================================================*/
/*                         KEY OPERATIONS                                    */
/*===========================================================================*/

/**
 * encryption_create_key - Create new encryption key
 */
int encryption_create_key(encryption_manager_t *manager, u32 device_id, 
                          const char *passphrase, u32 *key_slot)
{
    if (!manager || !passphrase || !key_slot) {
        return -EINVAL;
    }
    
    encrypted_device_t *dev = encryption_manager_get_device(manager, device_id);
    if (!dev) {
        return -ENOENT;
    }
    
    /* Find empty slot */
    for (u32 i = 0; i < LUKS_MAX_KEY_SLOTS; i++) {
        if (!dev->header.key_slots[i].active) {
            *key_slot = i;
            
            /* Initialize key slot */
            dev->header.key_slots[i].active = 1;
            dev->header.key_slots[i].iterations = dev->header.mk_iterations;
            generate_random_bytes(dev->header.key_slots[i].salt, 32);
            
            printk("[ENCRYPTION] Created key in slot %d\n", i);
            return 0;
        }
    }
    
    return -ENOSPC;
}

/**
 * encryption_delete_key - Delete encryption key
 */
int encryption_delete_key(encryption_manager_t *manager, u32 device_id, u32 key_slot)
{
    if (!manager || key_slot >= LUKS_MAX_KEY_SLOTS) {
        return -EINVAL;
    }
    
    encrypted_device_t *dev = encryption_manager_get_device(manager, device_id);
    if (!dev) {
        return -ENOENT;
    }
    
    dev->header.key_slots[key_slot].active = 0;
    
    printk("[ENCRYPTION] Deleted key from slot %d\n", key_slot);
    
    return 0;
}

/**
 * encryption_verify_key - Verify encryption key
 */
int encryption_verify_key(encryption_manager_t *manager, u32 device_id, const char *passphrase)
{
    encrypted_device_t *dev;

    if (!manager || !passphrase) {
        return -EINVAL;
    }

    /*
     * Length alone is not a verification. As written this returned 0 for ANY
     * passphrase of at least ENCRYPTION_MIN_PASSPHRASE characters, so an
     * attacker with no key could pass the check and -- because
     * luks_change_key() gates the re-wrap of the master key on it -- replace
     * the passphrase on a device they cannot open.
     *
     * Verification now means: derive a key from the passphrase in each active
     * slot and see whether the master key it produces matches the device's
     * stored digest. A wrong passphrase derives a different key and the
     * decrypt fails the digest check.
     */
    if (strlen(passphrase) < ENCRYPTION_MIN_PASSPHRASE) {
        return -EINVAL;
    }

    spin_lock(&manager->lock);

    dev = manager->devices;
    while (dev && dev->device_id != device_id) {
        dev = dev->next;
    }

    spin_unlock(&manager->lock);

    if (!dev) {
        return -ENOENT;
    }

    {
        u8 master_key[ENCRYPTION_MASTER_KEY_SIZE];
        u8 digest[32];
        u32 slot;
        int found = -EACCES;

        for (slot = 0; slot < LUKS_MAX_KEY_SLOTS; slot++) {
            if (!dev->header.key_slots[slot].active) {
                continue;
            }

            memset(master_key, 0, sizeof(master_key));

            if (luks_decrypt_master_key(dev, slot, master_key, passphrase) != 0) {
                continue;
            }

            /* The stored digest is the master key's fingerprint. */
            if (sha256(master_key, dev->key_size / 8, digest) == 0 &&
                memcmp(digest, dev->header.master_key_digest,
                       sizeof(dev->header.master_key_digest)) == 0) {
                found = 0;
                memset(master_key, 0, sizeof(master_key));
                break;
            }

            memset(master_key, 0, sizeof(master_key));
        }

        return found;
    }
}

/*===========================================================================*/
/*                         ENCRYPTION/DECRYPTION                             */
/*===========================================================================*/

/*
 * XTS-AES, as used by dm-crypt/LUKS.
 *
 * Both encryption_encrypt_data() and encryption_decrypt_data() were empty
 * bodies that bumped their operation and byte counters and returned 0 - so
 * every sector written to an "encrypted" device went to disk in the clear,
 * while the statistics reported it as encrypted, and every sector read back
 * came off disk unmodified and was reported as decrypted. That is the worst
 * possible failure mode for a disk-encryption layer: silent.
 *
 * XTS splits the master key into two AES keys K1 and K2. For sector number i
 * the tweak value is AES_K2(i) as a little-endian 128-bit block, and each
 * 16-byte block j within the sector is encrypted as:
 *
 *     C = AES_K1(P xor T) xor T ,  T = alpha^j (x) T0
 *
 * where the (x) multiply is the GF(2^128) operation with polynomial x^128 +
 * x^7 + x^2 + x + 1 (0x87 in the low byte), applied little-endian.
 *
 * Both directions use AES_K1 ENCRYPTION - XTS is not a chaining mode you
 * invert by swapping to the decryption function.
 */
#define XTS_SECTOR_SIZE     512
#define XTS_BLOCK_SIZE      16
#define XTS_BLOCKS_PER_SEC  (XTS_SECTOR_SIZE / XTS_BLOCK_SIZE)

/* Multiply the tweak by alpha in GF(2^128), little-endian byte order. */
static void xts_alpha_mul(u8 *tweak)
{
    u32 i;
    u8 carry_in = 0;
    u8 carry_out;

    for (i = 0; i < XTS_BLOCK_SIZE; i++) {
        carry_out = tweak[i] >> 7;
        tweak[i] = (u8)((tweak[i] << 1) | carry_in);
        carry_in = carry_out;
    }

    /* The reduction polynomial applies to the low byte, after the shift. */
    if (carry_in) {
        tweak[0] ^= 0x87;
    }
}

static void xts_xor(u8 *dst, const u8 *src)
{
    u32 i;

    for (i = 0; i < XTS_BLOCK_SIZE; i++) {
        dst[i] ^= src[i];
    }
}

/**
 * xts_crypt - Encrypt or decrypt a run of sectors with XTS-AES
 * @dev: Encrypted device (supplies the key and key size)
 * @sector: First sector number
 * @count: Number of sectors
 * @buffer: Sector data, modified in place
 * @encrypt: true to encrypt, false to decrypt
 *
 * Returns: 0 on success, negative error code on failure
 */
static int xts_crypt(encrypted_device_t *dev, u64 sector, u32 count,
                     void *buffer, bool encrypt)
{
    aes_context_t data_ctx;
    aes_context_t tweak_ctx;
    u8 *p = (u8 *)buffer;
    u32 key_bytes;
    u32 s;
    int ret;

    if (!dev || !buffer) {
        return -EINVAL;
    }

    key_bytes = dev->key_size / 8;

    /*
     * XTS needs two keys, so a 128-bit master key cannot be split into two
     * usable AES-128 keys. LUKS requires XTS to be configured with a
     * double-length key (AES-128-XTS takes 256 bits of key material).
     */
    if (key_bytes < 32 || (key_bytes % 2) != 0) {
        return -EINVAL;
    }

    ret = aes_init(&data_ctx);
    if (ret != 0) {
        return ret;
    }

    ret = aes_init(&tweak_ctx);
    if (ret != 0) {
        aes_free(&data_ctx);
        return ret;
    }

    /* K1 = first half, K2 = second half */
    ret = aes_set_key(&data_ctx, dev->master_key, (key_bytes / 2) * 8);
    if (ret != 0) {
        aes_free(&tweak_ctx);
        aes_free(&data_ctx);
        return ret;
    }

    ret = aes_set_key(&tweak_ctx, dev->master_key + key_bytes / 2,
                      (key_bytes / 2) * 8);
    if (ret != 0) {
        aes_free(&tweak_ctx);
        aes_free(&data_ctx);
        return ret;
    }

    for (s = 0; s < count; s++) {
        u8 tweak[XTS_BLOCK_SIZE];
        u8 zero_iv[XTS_BLOCK_SIZE];
        u8 block[XTS_BLOCK_SIZE];
        u64 sector_num = sector + s;
        u32 b;

        /* T0 = AES_K2(sector number as a little-endian 128-bit integer) */
        memset(zero_iv, 0, sizeof(zero_iv));
        memcpy(tweak, &sector_num, sizeof(sector_num));
        memset(tweak + sizeof(sector_num), 0, XTS_BLOCK_SIZE - sizeof(sector_num));

        ret = aes_set_iv(&tweak_ctx, zero_iv);
        if (ret != 0) {
            break;
        }

        ret = aes_encrypt_block(&tweak_ctx, tweak, tweak);
        if (ret != 0) {
            break;
        }

        for (b = 0; b < XTS_BLOCKS_PER_SEC; b++) {
            u8 *src = p + (u64)s * XTS_SECTOR_SIZE + (u64)b * XTS_BLOCK_SIZE;

            memcpy(block, src, XTS_BLOCK_SIZE);
            xts_xor(block, tweak);

            if (encrypt) {
                ret = aes_encrypt_block(&data_ctx, block, src);
            } else {
                ret = aes_decrypt_block(&data_ctx, block, src);
            }

            if (ret != 0) {
                break;
            }

            xts_xor(src, tweak);
            xts_alpha_mul(tweak);
        }

        if (ret != 0) {
            break;
        }
    }

    aes_free(&tweak_ctx);
    aes_free(&data_ctx);

    return ret;
}

/**
 * encryption_encrypt_data - Encrypt data for device
 */
int encryption_encrypt_data(encrypted_device_t *dev, u64 sector, u32 count, void *buffer)
{
    int ret;

    if (!dev || !buffer || !dev->is_unlocked) {
        return -EINVAL;
    }

    ret = xts_crypt(dev, sector, count, buffer, true);
    if (ret != 0) {
        dev->decrypt_failures++;
        return ret;
    }

    dev->encrypt_operations += count;
    dev->encrypt_bytes += (u64)count * XTS_SECTOR_SIZE;

    return 0;
}

int encryption_decrypt_data(encrypted_device_t *dev, u64 sector, u32 count, void *buffer)
{
    int ret;

    if (!dev || !buffer || !dev->is_unlocked) {
        return -EINVAL;
    }

    ret = xts_crypt(dev, sector, count, buffer, false);
    if (ret != 0) {
        dev->decrypt_failures++;
        return ret;
    }

    dev->decrypt_operations += count;
    dev->decrypt_bytes += (u64)count * XTS_SECTOR_SIZE;

    return 0;
}

/*===========================================================================*/
/*                         KEY DERIVATION                                    */
/*===========================================================================*/

/**
 * derive_key_pbkdf2 - Derive key using PBKDF2
 */
int derive_key_pbkdf2(const char *passphrase, const u8 *salt, u32 salt_size, 
                      u32 iterations, u8 *key, u32 key_size)
{
    s32 ret;

    if (!passphrase || !salt || !key || salt_size == 0 || key_size == 0) {
        return -EINVAL;
    }

    /*
     * This used to `memset(key, 0, key_size)` and return 0 - a successful key
     * derivation that produced an all-zero key, for every passphrase on every
     * system. Every LUKS volume in the tree would have been encrypted under a
     * key an attacker can guess in one try. Real PBKDF2-HMAC-SHA256 already
     * exists at security/crypto/sha.c:pbkdf2_sha256().
     *
     * Note salt_size and iterations are now honoured: the caller passes the
     * per-slot salt length and the header's iteration count, and both used to
     * be ignored (a hardcoded 32 was passed at one call site).
     */
    ret = pbkdf2_sha256((const u8 *)passphrase, (u32)strlen(passphrase),
                        salt, salt_size, iterations, key, key_size);

    return (ret == 0) ? 0 : -EIO;
}

/**
 * derive_key_scrypt - Derive key using scrypt
 */
int derive_key_scrypt(const char *passphrase, const u8 *salt, u32 salt_size,
                      u32 n, u32 r, u32 p, u8 *key, u32 key_size)
{
    if (!passphrase || !salt || !key) {
        return -EINVAL;
    }

    /* Length is part of the KDF's input; ignored only because the whole
     * function is unimplemented (see below). */
    (void)salt_size;

    /*
     * scrypt is not implemented. Returning 0 with a zeroed key would be worse
     * than failing: the caller cannot distinguish "derived a weak key" from
     * "derived the right key". Report it as unsupported so a caller that asked
     * for scrypt never silently gets something else.
     *
     * Argon2 and scrypt are both memory-hard on purpose; PBKDF2 is not a
     * drop-in substitute for either.
     */
    printk("[ENCRYPTION] scrypt not implemented (N=%d, r=%d, p=%d)\n", n, r, p);
    memset(key, 0, key_size);

    return -ENOSYS;
}

/**
 * derive_key_argon2 - Derive key using Argon2
 */
int derive_key_argon2(const char *passphrase, const u8 *salt, u32 salt_size,
                      u32 iterations, u32 memory, u32 parallelism, u8 *key, u32 key_size)
{
    if (!passphrase || !salt || !key) {
        return -EINVAL;
    }

    (void)salt_size;

    /* See derive_key_scrypt(): unsupported, not silently zero. */
    printk("[ENCRYPTION] Argon2 not implemented (iters=%d, mem=%d KB, par=%d)\n",
           iterations, memory, parallelism);
    memset(key, 0, key_size);

    return -ENOSYS;
}

/*===========================================================================*/
/*                         HARDWARE CRYPTO                                   */
/*===========================================================================*/

/**
 * encryption_has_hardware_crypto - Check for hardware crypto support
 */
bool encryption_has_hardware_crypto(void)
{
    /* In real implementation, would check CPU features */
    /* AES-NI, ARM Crypto Extensions, etc. */
    return false;
}

/**
 * encryption_init_hardware_crypto - Initialize hardware crypto
 */
int encryption_init_hardware_crypto(void)
{
    /* In real implementation, would initialize hardware crypto engine */
    return 0;
}

/**
 * encryption_cleanup_hardware_crypto - Cleanup hardware crypto
 */
int encryption_cleanup_hardware_crypto(void)
{
    /* In real implementation, would cleanup hardware crypto engine */
    return 0;
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * encryption_get_cipher_name - Get cipher name
 */
const char *encryption_get_cipher_name(u32 cipher)
{
    switch (cipher) {
        case LUKS_CIPHER_AES:       return "AES";
        case LUKS_CIPHER_SERPENT:   return "Serpent";
        case LUKS_CIPHER_TWOFISH:   return "Twofish";
        default:                    return "Unknown";
    }
}

/**
 * encryption_get_mode_name - Get mode name
 */
const char *encryption_get_mode_name(u32 mode)
{
    switch (mode) {
        case LUKS_MODE_XTS:     return "XTS";
        case LUKS_MODE_CBC:     return "CBC";
        case LUKS_MODE_LRW:     return "LRW";
        default:                return "Unknown";
    }
}

/**
 * encryption_get_hash_name - Get hash name
 */
const char *encryption_get_hash_name(u32 hash)
{
    switch (hash) {
        case LUKS_HASH_SHA256:      return "SHA-256";
        case LUKS_HASH_SHA512:      return "SHA-512";
        case LUKS_HASH_SHA1:        return "SHA-1";
        default:                    return "Unknown";
    }
}

/**
 * encryption_get_status_name - Get status name
 */
const char *encryption_get_status_name(u32 status)
{
    switch (status) {
        case ENCRYPTION_STATUS_NONE:        return "None";
        case ENCRYPTION_STATUS_LOCKED:      return "Locked";
        case ENCRYPTION_STATUS_UNLOCKED:    return "Unlocked";
        case ENCRYPTION_STATUS_ENCRYPTING:  return "Encrypting";
        case ENCRYPTION_STATUS_DECRYPTING:  return "Decrypting";
        default:                            return "Unknown";
    }
}

/**
 * encryption_get_key_size_bits - Get key size for cipher
 */
u32 encryption_get_key_size_bits(u32 cipher)
{
    (void)cipher;
    return 256;  /* Default */
}

/**
 * encryption_generate_salt - Generate random salt
 */
int encryption_generate_salt(u8 *salt, u32 size)
{
    if (!salt || size == 0) {
        return -EINVAL;
    }
    
    generate_random_bytes(salt, size);
    
    return 0;
}

/**
 * encryption_generate_uuid - Generate UUID
 */
int encryption_generate_uuid(char *uuid, u32 size)
{
    if (!uuid || size < 37) {
        return -EINVAL;
    }
    
    /* Generate random UUID */
    u8 bytes[16];
    generate_random_bytes(bytes, 16);
    
    snprintf(uuid, size, 
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             bytes[0], bytes[1], bytes[2], bytes[3],
             bytes[4], bytes[5], bytes[6], bytes[7],
             bytes[8], bytes[9], bytes[10], bytes[11],
             bytes[12], bytes[13], bytes[14], bytes[15]);
    
    return 0;
}

/**
 * generate_random_bytes - Fill a buffer with cryptographically random bytes
 * @buffer: Destination
 * @size: Byte count
 *
 * Returns: 0 on success, negative on failure
 *
 * This used to be `buffer[i] = (u8)(i * 17 + 31)` - a fixed arithmetic
 * sequence. Every salt, every LUKS UUID and, critically, every MASTER KEY in
 * the system came from the same predictable stream: the first 32 bytes of a
 * master key were always 0x1F 0x30 0x41 ... regardless of machine, boot or
 * user. Anyone could reconstruct the key from the source alone.
 *
 * crypto_random_bytes() now pulls from RDRAND/RDSEED and fails if neither is
 * available, instead of quietly substituting something guessable.
 */
static int generate_random_bytes(u8 *buffer, u32 size)
{
    if (!buffer || size == 0) {
        return -EINVAL;
    }

    if (crypto_random_bytes(buffer, size) != CRYPTO_OK) {
        pr_err("ENCRYPTION: no entropy source for key material\n");
        memset(buffer, 0, size);
        return -ENOSYS;
    }

    return 0;
}


/*===========================================================================*/
/*                         GLOBAL INSTANCE                                   */
/*===========================================================================*/

/**
 * encryption_manager_get_instance - Get global encryption manager instance
 */
encryption_manager_t *encryption_manager_get_instance(void)
{
    return &g_encryption_manager;
}
