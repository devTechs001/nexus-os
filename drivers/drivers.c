/*
 * NEXUS OS - Hardware Drivers Subsystem
 * drivers/drivers.c
 */

#include "../kernel/include/kernel.h"

/* Early driver initializers */
extern void serial_init(void);
extern int pci_init(void);
extern void rtc_init(void);

/* Late driver initializers */
extern void usb_init(void);
extern void audio_init(void);
extern void watchdog_init(void);
extern void sensor_hub_init(void);

/**
 * driver_early_init - Initialize essential early drivers
 *
 * Called before most kernel subsystems are up.
 */
void driver_early_init(void)
{
    pr_info("Initializing early drivers...\n");

    /* 1. Initialize serial for early debugging */
    serial_init();

    /* 2. Initialize PCI bus */
    pci_init();

    /* 3. Initialize Real-Time Clock */
    rtc_init();

    pr_info("Early drivers initialized.\n");
}

/**
 * driver_late_init - Initialize non-essential late drivers
 *
 * Called after core subsystems and basic drivers are up.
 */
void driver_late_init(void)
{
    pr_info("Initializing late drivers...\n");

    /* 1. Initialize USB stack */
    usb_init();

    /* 2. Initialize Audio subsystem */
    audio_init();

    /* 3. Initialize Watchdog */
    watchdog_init();

    /* 4. Initialize Sensors */
    sensor_hub_init();

    pr_info("Late drivers initialized.\n");
}
