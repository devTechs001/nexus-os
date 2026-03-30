/*
 * NEXUS OS - eMMC Storage Driver
 * drivers/storage/emmc.c
 *
 * eMMC 5.1 compliant storage driver
 * Copyright (c) 2026 NEXUS Development Team
 */

#include "sd.h"
#include "../../kernel/include/types.h"
#include "../../kernel/include/kernel.h"

/**
 * emmc_init - Initialize eMMC storage driver
 *
 * Returns: 0 on success, error code on failure
 */
int emmc_init(void)
{
    pr_info("eMMC: Initializing eMMC 5.1 storage driver...\n");

    /* 
     * eMMC initialization is largely handled by the shared SD/MMC driver stack.
     * Here we perform eMMC-specific controller probing and card detection.
     */

    /* 1. Ensure the core SD/MMC driver is initialized */
    if (!sd_is_initialized()) {
        if (sd_init() != 0) {
            pr_err("eMMC: Failed to initialize core SD/MMC driver\n");
            return -EIO;
        }
    }

    /* 2. Probe for eMMC controllers */
    /* In a real implementation, we would scan PCI or Device Tree for eMMC controllers */
    pr_info("eMMC: Probing for eMMC controllers...\n");
    
    /* 3. Detect and initialize eMMC cards */
    /* This will call sd_init_emmc_card for any detected eMMC devices */
    int count = sd_probe();
    if (count < 0) {
        pr_err("eMMC: Controller probe failed\n");
        return count;
    }

    pr_info("eMMC: Detected %d eMMC controller(s)\n", count);
    
    /* 4. Register with block layer */
    /* The SD driver already handles block layer registration */

    pr_info("eMMC: Driver initialized successfully\n");
    return 0;
}

/**
 * emmc_shutdown - Shut down eMMC storage driver
 */
void emmc_shutdown(void)
{
    pr_info("eMMC: Shutting down driver...\n");
    /* Core shutdown handled by sd_shutdown */
}

/**
 * emmc_get_info - Get eMMC device information
 */
int emmc_get_info(u32 device_id, sd_device_t *info)
{
    sd_device_t *dev = sd_get_device(device_id);
    if (!dev || dev->card_type != CARD_TYPE_EMMC) {
        return -ENODEV;
    }

    if (info) {
        memcpy(info, dev, sizeof(sd_device_t));
    }
    return 0;
}

/**
 * sd_switch_partition - Switch eMMC hardware partition
 * @dev: eMMC device
 * @partition: Partition ID (0=User, 1=Boot1, 2=Boot2, 3=RPMB)
 *
 * Returns: 0 on success, error code on failure
 */
int sd_switch_partition(sd_device_t *dev, u32 partition)
{
    if (!dev || dev->card_type != CARD_TYPE_EMMC) {
        return -ENODEV;
    }

    if (partition > 3) {
        return -EINVAL;
    }

    pr_info("eMMC: Switching to partition %u...\n", partition);

    /* 
     * Switching partition on eMMC involves writing to the 
     * PARTITION_CONFIG field (179) of the EXT_CSD register.
     */
    sd_controller_t *ctrl = sd_get_controller(dev->controller_id);
    if (!ctrl) return -ENODEV;

    /* bits [2:0] are the partition selection */
    int ret = mmc_card_switch(ctrl, 3, 179, partition);
    if (ret == 0) {
        dev->current_partition = partition;
        pr_info("eMMC: Successfully switched to partition %u\n", partition);
    } else {
        pr_err("eMMC: Failed to switch partition to %u (error %d)\n", partition, ret);
    }

    return ret;
}

/**
 * sd_get_partition_info - Get size of eMMC hardware partition
 * @dev: eMMC device
 * @partition: Partition ID
 * @size: Output size in bytes
 *
 * Returns: 0 on success, error code on failure
 */
int sd_get_partition_info(sd_device_t *dev, u32 partition, u64 *size)
{
    if (!dev || dev->card_type != CARD_TYPE_EMMC || !size) {
        return -EINVAL;
    }

    switch (partition) {
        case 0: /* User Data */
            *size = dev->capacity;
            break;
        case 1: /* Boot Partition 1 */
        case 2: /* Boot Partition 2 */
            /* Size is in EXT_CSD[226] in 128KB units */
            *size = (u64)dev->ext_csd.boot_size_mult * 128 * 1024;
            break;
        case 3: /* RPMB */
            /* Size is in EXT_CSD[168] in 128KB units */
            *size = (u64)dev->ext_csd.rpmb_size_mult * 128 * 1024;
            break;
        default:
            return -EINVAL;
    }

    return 0;
}
