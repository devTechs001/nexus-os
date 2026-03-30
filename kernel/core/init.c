/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * init.c - Kernel Initialization
 */

#include "../include/kernel.h"

/* Forward declarations */
typedef void (*initcall_t)(void);
static void do_initcalls(void);

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
extern void pmm_init(void);
extern void vmm_init(void);
extern void heap_init(void);
extern void heap_early_init(void);
extern void slab_init(void);
extern void scheduler_init(void);
extern void process_init(void);
extern void ipc_init(void);
extern void sync_init(void);
extern void vfs_init(void);
extern void storage_init(void);
extern void network_init(void);
extern int protocol_init(void);
extern int inference_init(void);
extern int iot_init(void);
extern int container_init(void);
extern int telephony_init(void);
extern void security_core_init(void);
extern void power_init(void);
extern int hypervisor_init(void);
extern void syscall_init(void);
extern void graphics_init(void);
extern void acpi_init(void);
extern void interrupt_init(void);
extern void percpu_init(void);
extern void pmm_early_init(void);
extern void console_init(void);
extern void driver_early_init(void);
extern void driver_late_init(void);
extern int input_init(void);

/* Architecture-specific initialization */
extern void arch_early_init(void);
extern void arch_late_init(void);
extern void arch_init_cpus(void);
extern void arch_init_gdt(void);
extern void arch_init_idt(void);
extern void arch_init_paging(void);
extern void arch_init_timer(u64 freq);
extern void arch_init_smp(void);
extern void arch_enable_interrupts(void);

extern void create_init_process(void);

/**
 * init_process_start - Start the first userspace process (init)
 */
void init_process_start(void)
{
    pr_info("Starting init process (PID 1)...\n");
    create_init_process();
}

extern struct task_struct *task_create(const char *name, void (*fn)(void *), void *arg);
extern void task_wake_up(struct task_struct *task);

static void service_manager_thread(void *arg)
{
    (void)arg;
    pr_info("Service Manager started.\n");
    while (1) {
        /* Service management loop */
        delay_ms(1000);
    }
}

/**
 * service_manager_start - Start the system service manager
 */
void service_manager_start(void)
{
    pr_info("Starting service manager...\n");
    struct task_struct *sm = task_create("servicemanager", service_manager_thread, NULL);
    if (sm) {
        task_wake_up(sm);
    }
}

/* GUI and Userspace */
typedef struct login_screen login_screen_t;
typedef struct onboarding_wizard onboarding_wizard_t;

/* Use real instances from GUI modules if they exist, otherwise null stubs */
extern int gui_framework_init(void);
extern int desktop_init_global(void);
extern login_screen_t *login_screen_get_instance(void);
extern int login_screen_init(login_screen_t *screen);
extern int login_screen_run(login_screen_t *screen);
extern onboarding_wizard_t *onboarding_wizard_get_instance(void);
extern int onboarding_wizard_init(onboarding_wizard_t *wizard);
extern int onboarding_wizard_run(onboarding_wizard_t *wizard);

/* Initcall sections */
extern initcall_t __initcall_start[], __initcall_end[];

/*===========================================================================*/
/*                     KERNEL INITIALIZATION                                 */
/*===========================================================================*/

/* Kernel state access */
extern struct {
    const char *version;
    const char *codename;
    u64 boot_time;
    volatile u32 state;
    u64 mem_total;
    u64 mem_used;
    u32 cpu_count;
    bool smp_enabled;
    bool virt_enabled;
    bool is_vmware;
    bool is_virtualbox;
    bool is_qemu;
    bool is_hyperv;
    char virt_mode[16];
} kernel_state;

extern void detect_virtualization(void);

/**
 * kernel_init_early - Early kernel initialization
 */
void kernel_init_early(void)
{
    current_init_level = INIT_LEVEL_EARLY;
    
    pr_info("Early kernel initialization...\n");
    
    /* 1. Architecture-specific early init */
    arch_early_init();
    
    /* 2. Initialize console output */
    console_init();
    
    /* 3. Initialize early kernel memory */
    pmm_early_init();

    /* 4. Detect virtualization environment EARLY */
    detect_virtualization();

    /* 5. Basic CPU structures */
    pr_info("  Initializing GDT & IDT...\n");
    arch_init_gdt();
    arch_init_idt();

    /* 6. Print virtualization status */
    if (kernel_state.is_vmware) {
        pr_info("[VM] VMware environment detected\n");
    } else if (kernel_state.is_virtualbox) {
        pr_info("[VM] VirtualBox environment detected\n");
    } else if (kernel_state.is_qemu) {
        pr_info("[VM] QEMU environment detected\n");
    } else {
        pr_info("[OK] Running on bare metal (%s mode)\n", kernel_state.virt_mode);
    }
    
    current_init_level = INIT_LEVEL_CPU;
}

/**
 * kernel_init_cpus - Initialize CPU subsystems
 */
void kernel_init_cpus(void)
{
    pr_info("Initializing CPU subsystems...\n");
    
    /* 1. Initialize boot CPU */
    arch_init_cpus();
    
    /* 2. Initialize per-CPU data */
    percpu_init();
    
    /* 3. Initialize interrupt controllers */
    interrupt_init();
    
    /* 4. Initialize system timer (1 GHz default) */
    arch_init_timer(1000000000);

    /* 5. Initialize ACPI */
    acpi_init();

    /* 6. Initialize SMP */
    arch_init_smp();
    
    current_init_level = INIT_LEVEL_INTERRUPTS;
}

/**
 * kernel_init_subsystems - Initialize kernel subsystems
 */
void kernel_init_subsystems(void)
{
    pr_info("Initializing kernel subsystems...\n");
    
    /* 1. Memory management (Complete) */
    pr_info("  Memory management (PMM, Paging, VMM, Heap, Slab)...\n");
    pmm_init();
    arch_init_paging();
    vmm_init();
    heap_init();
    slab_init();
    
    /* 2. Scheduler */
    pr_info("  Scheduler...\n");
    scheduler_init();
    
    /* 3. Process management */
    pr_info("  Process management...\n");
    process_init();
    
    /* 4. IPC */
    pr_info("  IPC...\n");
    ipc_init();
    
    /* 5. Synchronization primitives */
    sync_init();
    
    /* 6. Syscall interface */
    syscall_init();

    /* 7. Enable interrupts globally */
    arch_enable_interrupts();
    
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
    if (protocol_init() != 0) {
        pr_err("  Failed to initialize network protocols.\n");
    }
    
    /* Graphics */
    graphics_init();
    
    /* Input devices */
    if (input_init() != 0) {
        pr_err("  Failed to initialize input devices.\n");
    }
    
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
    security_core_init();
    
    /* Power management */
    power_init();
    
    /* Virtualization */
    if (hypervisor_init() != 0) {
        pr_warn("  Failed to initialize hypervisor.\n");
    }
    
    /* AI/ML Inference */
    if (inference_init() != 0) {
        pr_err("  Failed to initialize inference engine.\n");
    }
    
    /* IoT Stack */
    if (iot_init() != 0) {
        pr_err("  Failed to initialize IoT stack.\n");
    }
    
    /* Containers */
    if (container_init() != 0) {
        pr_err("  Failed to initialize container runtime.\n");
    }
    
    /* Mobile Telephony */
    if (telephony_init() != 0) {
        pr_err("  Failed to initialize telephony manager.\n");
    }
    
    /* Execute registered initcalls */
    do_initcalls();
    
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
    
    /* 1. Start init process */
    init_process_start();
    
    /* 2. Start service manager */
    service_manager_start();

    /* 3. Initialize GUI components (Login / Onboarding) */
    boot_params_t *params = get_boot_params();
    if (params && params->graphics_mode) {
        pr_info("  Initializing GUI components...\n");
        
        /* Initialize GUI framework (Renderer, Compositor) */
        gui_framework_init();

        /* Initialize Desktop Environment */
        desktop_init_global();

        /* Check if onboarding is needed (first boot) */
        onboarding_wizard_t *wizard = onboarding_wizard_get_instance();
        if (wizard) {
            onboarding_wizard_init(wizard);
            onboarding_wizard_run(wizard);
        }

        /* Initialize login screen */
        login_screen_t *login = login_screen_get_instance();
        if (login) {
            login_screen_init(login);
            login_screen_run(login);
        }
    }
    
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
#define DEFINE_INITCALL(fn, level) \
    static initcall_t __initcall_##fn __section(".initcall_" #level) = fn

#define early_initcall(fn)      DEFINE_INITCALL(fn, 0)
#define core_initcall(fn)       DEFINE_INITCALL(fn, 1)
#define subsys_initcall(fn)     DEFINE_INITCALL(fn, 2)
#define device_initcall(fn)     DEFINE_INITCALL(fn, 3)
#define late_initcall(fn)       DEFINE_INITCALL(fn, 4)

/**
 * do_initcalls - Execute all registered initcalls
 * 
 * Executes all initialization functions registered via initcall macros.
 * This is called during kernel initialization to run all subsystem init functions.
 */
__attribute__((used))
static void do_initcalls(void)
{
    extern initcall_t __initcall_start[], __initcall_end[];
    initcall_t *call;
    u32 count = 0;

    pr_info("Executing initcalls...\n");
    
    for (call = __initcall_start; call < __initcall_end; call++) {
        if (*call) {
            (*call)();
            count++;
        }
    }
    
    pr_info("Executed %u initcalls\n", count);
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
    boot_params_t *params = get_boot_params();
    static char cmdline_buf[256];

    snprintf(cmdline_buf, sizeof(cmdline_buf),
             "graphics=%d text=%d safe=%d debug=%d",
             params->graphics_mode, params->text_mode,
             params->safe_mode, params->debug_mode);

    return cmdline_buf;
}

/**
 * check_kernel_option - Check for kernel option
 */
bool check_kernel_option(const char *option)
{
    boot_params_t *params = get_boot_params();

    if (strcmp(option, "graphics") == 0)
        return params->graphics_mode;
    if (strcmp(option, "text") == 0)
        return params->text_mode;
    if (strcmp(option, "safe") == 0 || strcmp(option, "nomodeset") == 0)
        return params->safe_mode;
    if (strcmp(option, "debug") == 0)
        return params->debug_mode;
    if (strcmp(option, "nosmp") == 0)
        return params->nosmp;
    if (strcmp(option, "noapic") == 0)
        return params->noapic;

    return false;
}
