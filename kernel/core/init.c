/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * init.c - Kernel Initialization
 */

#include "../include/kernel.h"

/*===========================================================================*/
/*                         INITIALIZATION LEVELS                             */
/*===========================================================================*/

/*
 * NEXUS OS uses a phased initialization approach:
 * 
 * Level 0: Early boot (bootloader handoff)
 * Level 1: CPU and basic memory
 * Level 2: Interrupts and exceptions
 * Level 3: Kernel subsystems
 * Level 4: Device drivers
 * Level 5: Userspace initialization
 */

typedef enum {
    INIT_LEVEL_EARLY      = 0,
    INIT_LEVEL_CPU        = 1,
    INIT_LEVEL_INTERRUPTS = 2,
    INIT_LEVEL_SUBSYSTEMS = 3,
    INIT_LEVEL_DRIVERS    = 4,
    INIT_LEVEL_USERSPACE  = 5,
} init_level_t;

static init_level_t current_init_level = INIT_LEVEL_EARLY;

/*===========================================================================*/
/*                     INITIALIZATION FUNCTIONS                              */
/*===========================================================================*/

/* External initialization functions */
extern void mm_init(void);
extern void scheduler_init(void);
extern void process_init(void);
extern void ipc_init(void);
extern void vfs_init(void);
extern void network_init(void);
extern void security_init(void);

/* Architecture-specific initialization */
extern void arch_early_init(void);
extern void arch_late_init(void);
extern void arch_init_cpus(void);

/* Driver initialization */
extern void driver_early_init(void);
extern void driver_late_init(void);

/*===========================================================================*/
/*                     KERNEL INITIALIZATION                                 */
/*===========================================================================*/

/**
 * kernel_init_early - Early kernel initialization
 * 
 * Called immediately after bootloader handoff. Sets up minimal
 * environment needed for further initialization.
 */
void kernel_init_early(void)
{
    current_init_level = INIT_LEVEL_EARLY;
    
    pr_info("Early kernel initialization...\n");
    
    /* Architecture-specific early init */
    arch_early_init();
    
    /* Initialize console output */
    printk_init();
    
    /* Initialize kernel memory */
    mm_early_init();
    
    current_init_level = INIT_LEVEL_CPU;
}

/**
 * kernel_init_cpus - Initialize CPU subsystems
 */
void kernel_init_cpus(void)
{
    pr_info("Initializing CPU subsystems...\n");
    
    /* Initialize boot CPU */
    arch_init_cpus();
    
    /* Initialize per-CPU data */
    percpu_init();
    
    /* Initialize interrupt controllers */
    interrupt_init();
    
    current_init_level = INIT_LEVEL_INTERRUPTS;
}

/**
 * kernel_init_subsystems - Initialize kernel subsystems
 */
void kernel_init_subsystems(void)
{
    pr_info("Initializing kernel subsystems...\n");
    
    /* Memory management */
    pr_info("  Memory management...\n");
    mm_init();
    
    /* Scheduler */
    pr_info("  Scheduler...\n");
    scheduler_init();
    
    /* Process management */
    pr_info("  Process management...\n");
    process_init();
    
    /* IPC */
    pr_info("  IPC...\n");
    ipc_init();
    
    /* Synchronization primitives */
    sync_init();
    
    /* Syscall interface */
    syscall_init();
    
    current_init_level = INIT_LEVEL_SUBSYSTEMS;
}

/**
 * kernel_init_drivers - Initialize device drivers
 */
void kernel_init_drivers(void)
{
    pr_info("Initializing device drivers...\n");
    
    /* Early driver initialization */
    driver_early_init();
    
    /* VFS */
    pr_info("  VFS...\n");
    vfs_init();
    
    /* Storage drivers */
    storage_init();
    
    /* Network */
    pr_info("  Network...\n");
    network_init();
    
    /* Graphics */
    graphics_init();
    
    /* Input devices */
    input_init();
    
    /* Late driver initialization */
    driver_late_init();
    
    current_init_level = INIT_LEVEL_DRIVERS;
}

/**
 * kernel_init_late - Late kernel initialization
 */
void kernel_init_late(void)
{
    pr_info("Late kernel initialization...\n");
    
    /* Security subsystem */
    security_init();
    
    /* Power management */
    power_init();
    
    /* Virtualization */
    hypervisor_init();
    
    /* Architecture-specific late init */
    arch_late_init();
    
    current_init_level = INIT_LEVEL_USERSPACE;
}

/**
 * kernel_init_userspace - Initialize userspace
 */
void kernel_init_userspace(void)
{
    pr_info("Initializing userspace...\n");
    
    /* Start init process */
    init_process_start();
    
    /* Start service manager */
    service_manager_start();
    
    pr_info("Userspace initialization complete.\n");
}

/*===========================================================================*/
/*                     INITIALIZATION SEQUENCE                               */
/*===========================================================================*/

/**
 * kernel_main_init - Main initialization sequence
 * 
 * This is the primary initialization flow called from kernel_main()
 */
void kernel_main_init(void)
{
    pr_info("\n");
    pr_info("===============================================\n");
    pr_info("  NEXUS OS Kernel Initialization\n");
    pr_info("===============================================\n\n");
    
    /* Phase 1: Early initialization */
    kernel_init_early();
    
    /* Phase 2: CPU and interrupts */
    kernel_init_cpus();
    
    /* Phase 3: Kernel subsystems */
    kernel_init_subsystems();
    
    /* Phase 4: Device drivers */
    kernel_init_drivers();
    
    /* Phase 5: Late initialization */
    kernel_init_late();
    
    /* Phase 6: Userspace */
    kernel_init_userspace();
    
    pr_info("\n");
    pr_info("===============================================\n");
    pr_info("  Kernel Initialization Complete!\n");
    pr_info("===============================================\n\n");
}

/*===========================================================================*/
/*                     INITIALIZATION HELPERS                                */
/*===========================================================================*/

/**
 * initcall - Register initialization function
 */
typedef void (*initcall_t)(void);

#define DEFINE_INITCALL(fn, level) \
    static initcall_t __initcall_##fn __section(".initcall_" #level) = fn

#define early_initcall(fn)      DEFINE_INITCALL(fn, 0)
#define core_initcall(fn)       DEFINE_INITCALL(fn, 1)
#define subsys_initcall(fn)     DEFINE_INITCALL(fn, 2)
#define device_initcall(fn)     DEFINE_INITCALL(fn, 3)
#define late_initcall(fn)       DEFINE_INITCALL(fn, 4)

/**
 * do_initcalls - Execute all registered initcalls
 */
static void do_initcalls(void)
{
    extern initcall_t __initcall_start[], __initcall_end[];
    initcall_t *call;
    
    for (call = __initcall_start; call < __initcall_end; call++) {
        (*call)();
    }
}

/*===========================================================================*/
/*                     MODULE INITIALIZATION                                 */
/*===========================================================================*/

#ifdef CONFIG_MODULES

/**
 * module_init - Module initialization macro
 */
#define module_init(fn) \
    static initcall_t __module_init_##fn __section(".module_init") = fn

/**
 * module_exit - Module cleanup macro
 */
#define module_exit(fn) \
    static exitcall_t __module_exit_##fn __section(".module_exit") = fn

#endif /* CONFIG_MODULES */

/*===========================================================================*/
/*                     BOOT PARAMETERS                                       */
/*===========================================================================*/

/**
 * boot_params - Kernel boot parameters
 */
static struct {
    char cmdline[1024];
    u32 magic;
    u32 flags;
} boot_params;

#define BOOT_MAGIC          0x12345678
#define BOOT_FLAG_DEBUG     0x00000001
#define BOOT_FLAG_SINGLE    0x00000002
#define BOOT_FLAG_INITRD    0x00000004

/**
 * parse_boot_params - Parse boot parameters
 */
static void parse_boot_params(void)
{
    pr_info("Boot command line: %s\n", boot_params.cmdline);
    
    /* Check for debug mode */
    if (boot_params.flags & BOOT_FLAG_DEBUG) {
        pr_info("Debug mode enabled\n");
    }
    
    /* Check for single-user mode */
    if (boot_params.flags & BOOT_FLAG_SINGLE) {
        pr_info("Single-user mode\n");
    }
}

/*===========================================================================*/
/*                     INITIALIZATION DEBUGGING                              */
/*===========================================================================*/

#ifdef CONFIG_DEBUG_INIT

static u64 init_times[16];
static u32 init_time_index = 0;

/**
 * init_trace - Trace initialization timing
 */
void init_trace(const char *msg)
{
    if (init_time_index < ARRAY_SIZE(init_times)) {
        init_times[init_time_index++] = get_time_ms();
        pr_debug("INIT [%u]: %s\n", init_time_index, msg);
    }
}

#define INIT_TRACE(msg) init_trace(msg)

#else
#define INIT_TRACE(msg)
#endif /* CONFIG_DEBUG_INIT */

/*===========================================================================*/
/*                     KERNEL COMMAND LINE                                   */
/*===========================================================================*/

/**
 * get_kernel_cmdline - Get kernel command line
 */
const char *get_kernel_cmdline(void)
{
    return boot_params.cmdline;
}

/**
 * check_kernel_option - Check for kernel option
 */
bool check_kernel_option(const char *option)
{
    return strstr(boot_params.cmdline, option) != NULL;
}
