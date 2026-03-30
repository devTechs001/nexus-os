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

    pr_info("Storage Subsystem initialized.\n");
}
