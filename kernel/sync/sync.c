/*
 * NEXUS OS - Synchronization Subsystem
 * kernel/sync/sync.c
 */

#include "sync.h"
#include "../include/kernel.h"

/* External subsystem initializers */
extern void spinlock_subsystem_init(void);
extern void rwlock_subsystem_init(void);
extern void waitqueue_subsystem_init(void);
extern void atomic_subsystem_init(void);

/**
 * sync_init - Initialize synchronization subsystem
 */
void sync_init(void)
{
    pr_info("Initializing synchronization subsystem...\n");

    /* 1. Initialize spinlock subsystem first (foundation) */
    spinlock_subsystem_init();

    /* 2. Initialize atomic operations */
    atomic_subsystem_init();

    /* 3. Initialize wait queues (for blocking sync) */
    waitqueue_subsystem_init();

    /* 4. Initialize RW locks */
    rwlock_subsystem_init();

    pr_info("Synchronization subsystem initialized.\n");
}
