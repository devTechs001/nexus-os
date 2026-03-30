/*
 * NEXUS OS - Inter-Process Communication
 * kernel/ipc/ipc.c
 */

#include "ipc.h"
#include "../include/kernel.h"

/* External component initializers */
extern void msgq_init(void);
extern void shm_init(void);
extern void pipe_init(void);
extern void sem_init(void);

/**
 * ipc_init - Initialize IPC subsystem
 *
 * Coordinates initialization of all IPC components.
 */
void ipc_init(void)
{
    pr_info("Initializing IPC subsystem...\n");

    /* Initialize individual components */
    msgq_init();
    shm_init();
    pipe_init();
    sem_init();

    pr_info("IPC subsystem initialized.\n");
}
