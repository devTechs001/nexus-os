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

/* Forward type declarations for GUI components */
typedef struct login_screen login_screen_t;
typedef struct onboarding_wizard onboarding_wizard_t;
struct compositor;

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
extern void splash_update_progress(u32 percent, const char *status);
extern void splash_done(void);
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
extern int nexus_app_launch(int app);   /* gui/apps_nexus.c */
#define NEXUS_APP_VM_MANAGER 1         /* matches nexus_app_t */
extern login_screen_t *login_screen_get_instance(void);
extern int login_screen_init(login_screen_t *screen);
extern int login_screen_run(login_screen_t *screen);
extern int login_screen_update(login_screen_t *screen);
extern onboarding_wizard_t *onboarding_wizard_get_instance(void);
extern int onboarding_wizard_init(onboarding_wizard_t *wizard);
extern int onboarding_wizard_run(onboarding_wizard_t *wizard);
extern int onboarding_wizard_update(onboarding_wizard_t *wizard);

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

    /* 2.5 Show Boot Splash Logo (Text Mode) */
    void splash_draw_logo_text(u32 x, u32 y);
    splash_draw_logo_text(0, 0);
    
    /* 3. Initialize early kernel memory */
    pmm_early_init();

    /* 4. Detect virtualization environment EARLY */
    detect_virtualization();

    /* 5. Basic CPU structures */
    pr_info("  Initializing GDT...\n");
    arch_init_gdt();

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
    
    /*
     * 4. Initialize system timer at 1000 Hz (1 ms tick).
     *
     * This used to pass 1000000000. The PIT divisor is
     * 1193182 / freq, which truncates to 0 at 1 GHz, and a divisor of 0
     * means 65536 to the hardware - so the PIT actually ticked at ~18 Hz
     * while the kernel believed it was 1 tick/ms. Every tick-based timeout
     * and the boot splash animation were therefore wrong by ~55x.
     */
    arch_init_timer(1000);

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
    pr_info("  [1/6] Memory management (PMM, Paging, VMM, Heap, Slab)...\n");
    pmm_init();
    
    outb(0x3F8, '1');
    arch_init_paging();
    
    outb(0x3F8, '2');
    vmm_init();
    
    outb(0x3F8, '3');
    heap_init();
    
    outb(0x3F8, '4');
    slab_init();
    
    outb(0x3F8, '5');
    pr_info("  [OK] Memory management initialized\n");
    
    /* 2. Scheduler */
    pr_info("  [2/6] Scheduler...\n");
    scheduler_init();
    pr_info("  [OK] Scheduler initialized\n");
    
    /* 3. Process management */
    pr_info("  [3/6] Process management...\n");
    process_init();
    pr_info("  [OK] Process management initialized\n");
    
    /* 4. IPC */
    pr_info("  [4/6] IPC...\n");
    ipc_init();
    pr_info("  [OK] IPC initialized\n");
    
    /* 5. Synchronization primitives */
    pr_info("  [5/6] Synchronization primitives...\n");
    sync_init();
    pr_info("  [OK] Synchronization initialized\n");
    
    /* 6. Syscall interface */
    pr_info("  [6/6] Syscall interface...\n");
    syscall_init();
    pr_info("  [OK] Syscall interface initialized\n");

    /* 7. Enable interrupts globally */
    pr_info("  Enabling interrupts globally...\n");
    arch_enable_interrupts();
    pr_info("  [OK] Interrupts enabled\n");
    
    current_init_level = INIT_LEVEL_SUBSYSTEMS;
}

/**
 * kernel_init_drivers - Initialize device drivers
 */
void kernel_init_drivers(void)
{
    pr_info("Initializing device drivers...\n");
    
    /* Graphics (Initialize early to show splash) */
    graphics_init();
    splash_update_progress(10, "Graphics subsystem initialized");

    /* Early driver initialization */
    driver_early_init();
    
    /* VFS */
    pr_info("  VFS...\n");
    vfs_init();
    splash_update_progress(30, "Virtual File System initialized");
    
    /* Storage drivers */
    storage_init();
    splash_update_progress(50, "Storage drivers initialized");
    
    /* Network */
    pr_info("  Network...\n");
    network_init();
    if (protocol_init() != 0) {
        pr_err("  Failed to initialize network protocols.\n");
        kernel_panic("Critical Failure: Network protocol initialization failed.");
    }
    splash_update_progress(60, "Network subsystem ready");
    
    /* Input devices */
    if (input_init() != 0) {
        pr_err("  Failed to initialize input devices.\n");
        kernel_panic("Critical Failure: Input device initialization failed.");
    }
    splash_update_progress(70, "Input devices ready");

    /* 6. Resource Sharing (VMware, VBox, QEMU guest tools) */
    typedef struct resource_sharing_config resource_sharing_config_t;
    extern int resource_sharing_init(resource_sharing_config_t *config);
    extern resource_sharing_config_t *resource_sharing_get_global_config(void);
    if (resource_sharing_init(resource_sharing_get_global_config()) != 0) {
        pr_warn("  Failed to initialize resource sharing.\n");
    }
    
    /* Late driver initialization */
    driver_late_init();
    
    splash_update_progress(80, "Kernel initialization complete");
    
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
    splash_update_progress(85, "Security subsystem initialized");
    
    /* Power management */
    power_init();
    
    /* Virtualization */
    if (hypervisor_init() != 0) {
        pr_warn("  Failed to initialize hypervisor.\n");
    }
    splash_update_progress(90, "System services started");
    
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
    
    splash_update_progress(95, "Preparing GUI...");
    
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
        pr_info("  [GUI] Starting Graphical User Interface...\n");

        /* Initialize GUI framework (Renderer, Compositor) */
        pr_info("  [GUI] Initializing Renderer and Compositor...\n");
        if (gui_framework_init() == 0) {
            pr_info("  [GUI] GUI Framework Ready\n");

            splash_done();

            /* Initialize Desktop Environment */
            pr_info("  [GUI] Initializing Desktop Environment...\n");
            desktop_init_global();

            /*
             * The Virtualization Manager is a self-contained full-screen
             * application. It used to have no caller at all, so it could
             * never be seen; with the "vmmanager" boot parameter it comes
             * up straight over the desktop, which also makes it reachable
             * on a system whose shell has no command parser yet.
             */
            if (params->vm_manager) {
                pr_info("  [GUI] Starting Virtualization Manager...\n");
                if (nexus_app_launch(NEXUS_APP_VM_MANAGER) != 0) {
                    pr_err("  [GUI] Virtualization Manager failed to start\n");
                }
            }

            /* Check if onboarding is needed (first boot) */
            onboarding_wizard_t *wizard = onboarding_wizard_get_instance();
            if (wizard) {
                pr_info("  [GUI] Starting Onboarding Wizard...\n");
                if (onboarding_wizard_init(wizard) != 0) {
                    pr_err("  [GUI] Failed to initialize onboarding wizard!\n");
                }
                onboarding_wizard_run(wizard);
            }

            /* Initialize login screen */
            login_screen_t *login = login_screen_get_instance();
            if (login) {
                pr_info("  [GUI] Starting Login Screen...\n");
                if (login_screen_init(login) != 0) {
                    pr_err("  [GUI] Failed to initialize login screen!\n");
                    kernel_panic("Critical Failure: Login screen initialization failed.");
                }
                login_screen_run(login);
            }

            /* 4. GUI Main Event Loop - Keep GUI running */
            pr_info("  [GUI] Entering GUI event loop...\n");
            bool gui_running = true;
            u64 last_frame_time = get_time_ms();
            u32 frame_count = 0;

            while (gui_running) {
                /* Calculate delta time */
                u64 current_time = get_time_ms();
                u64 delta_ms = current_time - last_frame_time;

                /* Limit to ~60 FPS (16ms per frame) */
                if (delta_ms < 16) {
                    /*
                     * Small pause to prevent busy-waiting. Do NOT use
                     * arch_halt_cpu() here: it executes cli; hlt, which
                     * stops the CPU with interrupts disabled and can never
                     * return, freezing the GUI on the first frame.
                     */
                    __asm__ __volatile__("pause" ::: "memory");
                    continue;
                }

                last_frame_time = current_time;
                frame_count++;

                /* Update GUI state */
                login_screen_update(login);
                onboarding_wizard_update(wizard);

                /* Present frame */
                extern struct compositor *compositor_get_instance(void);
                extern int compositor_present(struct compositor *comp);
                compositor_present(compositor_get_instance());

                /* Frame rate limiting */
                if (frame_count >= 60) {
                    pr_debug("[GUI] Running at %u FPS\n", frame_count);
                    frame_count = 0;
                }
            }

            pr_info("  [GUI] GUI event loop exited.\n");
        } else {
            pr_err("  [GUI] Failed to initialize GUI Framework! Falling back to console.\n");
        }
    } else {
        pr_info("  [BOOT] Staying in text mode as requested.\n");
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
    
    /* Phase 1: Early architecture initialization */
    kernel_init_early();
    
    /* Phase 2: CPU and interrupt subsystems */
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
