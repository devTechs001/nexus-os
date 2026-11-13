/*
 * NEXUS OS - Storage Subsystem
 * drivers/storage/storage.c
 */

#include "../../kernel/include/kernel.h"

/* External initializers from storage drivers */
extern int ahci_init(void);
extern int nvme_init(void);
extern int sd_init(void);
extern int emmc_init(void);
extern int ahci_register_block_devices(void);

#define STORAGE_MAX_BLOCK_DEVICES   16

typedef struct {
    bool  registered;
    char  name[32];
    u64   size_bytes;
    u32   block_size;

    /* Driver-specific handle passed back verbatim. */
    u32   handle;
    u32   sub_handle;

    int (*read)(u32 handle, u32 sub, u64 lba, u32 count, void *buf);
    int (*write)(u32 handle, u32 sub, u64 lba, u32 count, const void *buf);
} storage_block_device_t;

static struct {
    storage_block_device_t devices[STORAGE_MAX_BLOCK_DEVICES];
    u32 count;
    bool initialized;
} g_block_devs;

/**
 * storage_init - Initialize all storage controllers
 */
void storage_init(void)
{
    pr_info("Initializing Storage Subsystem...\n");

    /* 1. Initialize AHCI (SATA) controllers */
    ahci_init();

    /* 2. Initialize NVMe controllers */
    nvme_init();

    /* 3. Initialize SD/MMC controllers */
    sd_init();
    emmc_init();

    g_block_devs.count = 0;
    g_block_devs.initialized = true;

    /* Publish whatever the controllers found, so filesystems have a handle. */
    ahci_register_block_devices();

    pr_info("Storage Subsystem initialized.\n");
}

/*===========================================================================*/
/*                       GENERIC BLOCK DEVICE LAYER                          */
/*===========================================================================*/
/*
 * Each storage driver exposes its own block read/write with its own handle
 * arguments (AHCI needs a controller and port, NVMe a device id, SD a device
 * id). There was no layer above them, which is why NexFS had a
 * "void *bdev" field and every call around it was commented out - there was
 * nothing to call.
 *
 * This registry gives filesystems one handle and one pair of entry points.
 * Drivers register a backend at init time; storage_block_read/write dispatch
 * through it.
 */



/**
 * storage_block_register - Register a block backend
 *
 * @name:       Device name, e.g. "sda"
 * @handle:     Driver handle (controller id, device id, ...)
 * @sub:        Secondary handle (AHCI port); 0 where unused
 * @size:       Capacity in bytes
 * @blk_size:   Logical block size (0 means 512)
 *
 * Returns a device id >= 0, or a negative error code.
 */
int storage_block_register(const char *name, u32 handle, u32 sub, u64 size,
                           u32 blk_size,
                           int (*read)(u32, u32, u64, u32, void *),
                           int (*write)(u32, u32, u64, u32, const void *))
{
    storage_block_device_t *d;

    if (!name || !read) {
        return -EINVAL;
    }

    if (g_block_devs.count >= STORAGE_MAX_BLOCK_DEVICES) {
        return -ENOSPC;
    }

    d = &g_block_devs.devices[g_block_devs.count];

    u32 n = 0;
    while (name[n] && n + 1 < sizeof(d->name)) {
        d->name[n] = name[n];
        n++;
    }
    d->name[n] = '\0';

    d->registered = true;
    d->handle = handle;
    d->sub_handle = sub;
    d->size_bytes = size;
    d->block_size = blk_size ? blk_size : 512;
    d->read = read;
    d->write = write;

    pr_info("[STORAGE] Registered block device %s (%llu bytes, %u-byte blocks)\n",
            d->name, (unsigned long long)size, d->block_size);

    return (int)g_block_devs.count++;
}

/**
 * storage_block_read - Read blocks from a registered device
 *
 * @dev_id: Id returned by storage_block_register
 * @lba:    First block
 * @count:  Number of blocks
 * @buf:    Destination
 *
 * Returns 0 on success, negative error code on failure.
 */
int storage_block_read(u32 dev_id, u64 lba, u32 count, void *buf)
{
    storage_block_device_t *d;

    if (dev_id >= g_block_devs.count) {
        return -ENODEV;
    }

    d = &g_block_devs.devices[dev_id];
    if (!d->registered || !d->read) {
        return -ENODEV;
    }

    /* Refuse reads past the end rather than handing garbage to a filesystem. */
    if ((lba + count) * d->block_size > d->size_bytes) {
        return -EIO;
    }

    return d->read(d->handle, d->sub_handle, lba, count, buf);
}

/**
 * storage_block_write - Write blocks to a registered device
 */
int storage_block_write(u32 dev_id, u64 lba, u32 count, const void *buf)
{
    storage_block_device_t *d;

    if (dev_id >= g_block_devs.count) {
        return -ENODEV;
    }

    d = &g_block_devs.devices[dev_id];
    if (!d->registered || !d->write) {
        return -EROFS;
    }

    if ((lba + count) * d->block_size > d->size_bytes) {
        return -EIO;
    }

    return d->write(d->handle, d->sub_handle, lba, count, buf);
}

/**
 * storage_block_get_size - Capacity of a registered device in bytes
 */
u64 storage_block_get_size(u32 dev_id)
{
    if (dev_id >= g_block_devs.count || !g_block_devs.devices[dev_id].registered) {
        return 0;
    }
    return g_block_devs.devices[dev_id].size_bytes;
}

/**
 * storage_block_get_block_size - Logical block size of a device
 */
u32 storage_block_get_block_size(u32 dev_id)
{
    if (dev_id >= g_block_devs.count || !g_block_devs.devices[dev_id].registered) {
        return 0;
    }
    return g_block_devs.devices[dev_id].block_size;
}

/**
 * storage_block_device_count - Number of registered devices
 */
u32 storage_block_device_count(void)
{
    return g_block_devs.count;
}

/**
 * storage_block_device_name - Name of a registered device, or NULL
 */
const char *storage_block_device_name(u32 dev_id)
{
    if (dev_id >= g_block_devs.count || !g_block_devs.devices[dev_id].registered) {
        return NULL;
    }
    return g_block_devs.devices[dev_id].name;
}
