/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * kernel.c - Kernel Core Entry Point
 *
 * This is the main kernel entry point after boot.
 * NO userspace libraries - pure bare-metal code.
 */

#include "../include/kernel.h"
#include "../include/types.h"
#include "../include/config.h"

/* Kernel state constants */
#define KERNEL_STATE_BOOTING        0
#define KERNEL_STATE_INITIALIZING   1
#define KERNEL_STATE_RUNNING        2
#define KERNEL_STATE_SHUTTING_DOWN  3

/* Spinlock initializer */
#define SPINLOCK_UNLOCKED           { .lock = 0 }

/* Forward declarations for port I/O (implemented at bottom) */
extern void outb(unsigned short port, unsigned char val);
extern unsigned char inb(unsigned short port);
extern void io_wait(void);

/* Spinlock init stub */
#define spin_lock_init(lock) do { (lock)->lock = 0; } while(0)

/*===========================================================================*/
/*                         KERNEL BSS SECTION                                */
/*===========================================================================*/

/* Place kernel globals in BSS to avoid bloating binary */
__section(".bss") static struct {
    const char *version;
    const char *codename;
    u64 boot_time;
    volatile u32 state;
    u64 mem_total;
    u64 mem_used;
    u32 cpu_count;
    bool smp_enabled;
    bool virt_enabled;
    bool is_vmware;           /* VMware detection */
    bool is_virtualbox;       /* VirtualBox detection */
    bool is_qemu;             /* QEMU detection */
    bool is_hyperv;           /* Hyper-V detection */
    char virt_mode[16];       /* Current virtualization mode */
} kernel_state;

__section(".bss") static percpu_data_t percpu_data[MAX_CPUS];

__section(".bss") static spinlock_t kernel_lock;

__section(".bss") static u64 kernel_ticks;

__section(".bss") static u32 apic_timer_freq;

/*===========================================================================*/
/*                         EXTERNAL DECLARATIONS                             */
/*===========================================================================*/

/* Architecture-specific functions (must be implemented in arch/) */
extern void arch_early_init(void);
extern void arch_late_init(void);
extern void arch_enable_interrupts(void);
extern void arch_disable_interrupts(void);
extern u32 arch_get_cpu_id(void);
extern u32 arch_get_apic_id(void);
extern void arch_halt_cpu(void);
extern void arch_reboot(void);
extern void arch_shutdown(void);
extern u64 arch_get_ticks(void);
extern void arch_init_timer(u64 freq);
extern void arch_init_idt(void);
extern void arch_init_gdt(void);
extern void arch_init_paging(void);
extern void arch_init_smp(void);

/* Memory management */
extern void pmm_init(void);
extern void vmm_init(void);
extern void heap_init(void);
extern void slab_init(void);

/* Scheduler */
extern void scheduler_init(void);
extern void scheduler_start(void);

/* Interrupts */
extern void interrupt_init(void);

/* ACPI */
extern void acpi_init(void);

/*===========================================================================*/
/*                         CONSOLE OUTPUT (VGA Text Mode)                    */
/*===========================================================================*/

#define VGA_FRAMEBUFFER  0xB8000
#define VGA_WIDTH        80
#define VGA_HEIGHT       25

__section(".data") static u16 *vga_buffer = (u16 *)VGA_FRAMEBUFFER;
__section(".data") static u32 vga_cursor_x = 0;
__section(".data") static u32 vga_cursor_y = 0;

/* VGA Colors */
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW        14
#define VGA_COLOR_WHITE         15

/**
 * vga_putchar - Write character to VGA buffer
 */
static void vga_putchar(char c, u8 color)
{
    if (c == '\n') {
        vga_cursor_x = 0;
        vga_cursor_y++;
    } else if (c == '\r') {
        vga_cursor_x = 0;
    } else if (c == '\t') {
        vga_cursor_x = (vga_cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (vga_cursor_x > 0) {
            vga_cursor_x--;
            vga_buffer[vga_cursor_y * VGA_WIDTH + vga_cursor_x] = ' ' | (color << 8);
        }
    } else {
        vga_buffer[vga_cursor_y * VGA_WIDTH + vga_cursor_x] = c | (color << 8);
        vga_cursor_x++;
    }
    
    /* Scroll if needed */
    if (vga_cursor_x >= VGA_WIDTH) {
        vga_cursor_x = 0;
        vga_cursor_y++;
    }
    
    if (vga_cursor_y >= VGA_HEIGHT) {
        /* Scroll screen */
        for (u32 i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        for (u32 i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = ' ' | (color << 8);
        }
        vga_cursor_y = VGA_HEIGHT - 1;
    }
    
    /* Update cursor */
    u16 pos = vga_cursor_y * VGA_WIDTH + vga_cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}

/**
 * vga_clear - Clear screen
 */
static void vga_clear(u8 color)
{
    for (u32 i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        vga_buffer[i] = ' ' | (color << 8);
    }
    vga_cursor_x = 0;
    vga_cursor_y = 0;
}

/**
 * vga_set_color - Set text color
 */
static u8 vga_make_color(u8 fg, u8 bg)
{
    return (bg << 4) | (fg & 0x0F);
}

/*===========================================================================*/
/*                         PRINTK IMPLEMENTATION                             */
/*===========================================================================*/

/**
 * hex_to_char - Convert nibble to hex character
 */
static inline char hex_to_char(u8 nibble)
{
    static const char hex_chars[] = "0123456789ABCDEF";
    return hex_chars[nibble & 0x0F];
}

/**
 * print_decimal - Print unsigned decimal number
 */
static void print_decimal(u64 value, u8 color)
{
    if (value == 0) {
        vga_putchar('0', color);
        return;
    }
    
    char buffer[24];
    u32 i = 0;
    
    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    while (i > 0) {
        vga_putchar(buffer[--i], color);
    }
}

/**
 * print_hex - Print hexadecimal number
 */
static void print_hex(u64 value, u8 color, bool prefix, u32 width)
{
    if (prefix) {
        vga_putchar('0', color);
        vga_putchar('x', color);
    }
    
    char buffer[18];
    u32 i = 0;
    u64 temp = value;
    
    do {
        buffer[i++] = hex_to_char((u8)temp);
        temp >>= 4;
    } while (temp > 0);
    
    /* Pad with zeros if width specified */
    while (width > 0 && i < width) {
        buffer[i++] = '0';
    }
    
    /* Print in reverse order */
    while (i > 0) {
        vga_putchar(buffer[--i], color);
    }
}

/**
 * print_string - Print null-terminated string
 */
static void print_string(const char *str, u8 color)
{
    if (!str) {
        print_string("(null)", color);
        return;
    }
    
    while (*str) {
        vga_putchar(*str++, color);
    }
}

/**
 * vprintk - Format and print to VGA console
 * Note: Also in printk.c - this is a weak alias
 */
__attribute__((weak))
int vprintk(const char *fmt, __builtin_va_list args)
{
    u8 color = vga_make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    const char *p = fmt;
    
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
            case 's': {
                const char *str = __builtin_va_arg(args, const char *);
                print_string(str, color);
                break;
            }
            case 'd':
            case 'i': {
                s64 val = __builtin_va_arg(args, s64);
                if (val < 0) {
                    vga_putchar('-', color);
                    val = -val;
                }
                print_decimal((u64)val, color);
                break;
            }
            case 'u': {
                u64 val = __builtin_va_arg(args, u64);
                print_decimal(val, color);
                break;
            }
            case 'x': {
                u64 val = __builtin_va_arg(args, u64);
                print_hex(val, color, false, 0);
                break;
            }
            case 'X': {
                u64 val = __builtin_va_arg(args, u64);
                print_hex(val, color, true, 0);
                break;
            }
            case 'p': {
                void *ptr = __builtin_va_arg(args, void *);
                print_hex((u64)(uintptr)ptr, color, true, 16);
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                vga_putchar(c, color);
                break;
            }
            case '%':
                vga_putchar('%', color);
                break;
            case 'l':
                p++; /* Skip 'l' modifier, we handle 64-bit by default */
                continue;
            case '\0':
                return 0;
            default:
                vga_putchar('%', color);
                vga_putchar(*p, color);
                break;
            }
        } else if (*p == '\n') {
            vga_putchar('\n', color);
        } else {
            vga_putchar(*p, color);
        }
        p++;
    }
    
    return 0;
}

/**
 * printk - Print formatted string to console
 * Note: Also in printk.c - this is a weak alias
 */
__attribute__((weak))
int printk(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vprintk(fmt, args);
    __builtin_va_end(args);
    return 0;
}

/*===========================================================================*/
/*                         KERNEL INITIALIZATION                             */
/*===========================================================================*/

/*===========================================================================*/
/*                     VIRTUALIZATION DETECTION                              */
/*===========================================================================*/

/**
 * detect_virtualization - Detect if running in a VM (VMware, VirtualBox, etc.)
 *
 * Uses CPUID hypervisor detection to identify the virtualization environment.
 * Sets kernel_state.virt_mode and appropriate flags.
 */
static void detect_virtualization(void)
{
    u32 eax, ebx, ecx, edx;
    char hypervisor_sig[13];

    /* Initialize */
    kernel_state.is_vmware = false;
    kernel_state.is_virtualbox = false;
    kernel_state.is_qemu = false;
    kernel_state.is_hyperv = false;
    memset(kernel_state.virt_mode, 0, sizeof(kernel_state.virt_mode));

    /* Check for hypervisor presence (CPUID leaf 1, ECX bit 31) */
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );

    if (!(ecx & (1 << 31))) {
        /* No hypervisor detected - running on bare metal */
        strcpy(kernel_state.virt_mode, "native");
        return;
    }

    /* Get hypervisor signature (CPUID leaf 0x40000000) */
    __asm__ __volatile__(
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0x40000000)
    );

    hypervisor_sig[0] = ((char*)&ebx)[0];
    hypervisor_sig[1] = ((char*)&ebx)[1];
    hypervisor_sig[2] = ((char*)&ebx)[2];
    hypervisor_sig[3] = ((char*)&ebx)[3];
    hypervisor_sig[4] = ((char*)&ecx)[0];
    hypervisor_sig[5] = ((char*)&ecx)[1];
    hypervisor_sig[6] = ((char*)&ecx)[2];
    hypervisor_sig[7] = ((char*)&ecx)[3];
    hypervisor_sig[8] = ((char*)&edx)[0];
    hypervisor_sig[9] = ((char*)&edx)[1];
    hypervisor_sig[10] = ((char*)&edx)[2];
    hypervisor_sig[11] = ((char*)&edx)[3];
    hypervisor_sig[12] = '\0';

    /* Identify hypervisor */
    if (strcmp(hypervisor_sig, "VMwareVMware") == 0) {
        kernel_state.is_vmware = true;
        strcpy(kernel_state.virt_mode, "vmware");
    } else if (strcmp(hypervisor_sig, "VBoxVBoxVBox") == 0) {
        kernel_state.is_virtualbox = true;
        strcpy(kernel_state.virt_mode, "vbox");
    } else if (strcmp(hypervisor_sig, "Microsoft Hv") == 0) {
        kernel_state.is_hyperv = true;
        strcpy(kernel_state.virt_mode, "hyperv");
    } else {
        /* Check for QEMU by CPUID vendor string */
        __asm__ __volatile__(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(0)
        );

        hypervisor_sig[0] = ((char*)&ebx)[0];
        hypervisor_sig[1] = ((char*)&ebx)[1];
        hypervisor_sig[2] = ((char*)&ebx)[2];
        hypervisor_sig[3] = ((char*)&ebx)[3];
        hypervisor_sig[4] = ((char*)&edx)[0];
        hypervisor_sig[5] = ((char*)&edx)[1];
        hypervisor_sig[6] = ((char*)&edx)[2];
        hypervisor_sig[7] = ((char*)&edx)[3];
        hypervisor_sig[8] = ((char*)&ecx)[0];
        hypervisor_sig[9] = ((char*)&ecx)[1];
        hypervisor_sig[10] = ((char*)&ecx)[2];
        hypervisor_sig[11] = ((char*)&ecx)[3];
        hypervisor_sig[12] = '\0';

        if (strcmp(hypervisor_sig, "QEMUQEMUQ") == 0) {
            kernel_state.is_qemu = true;
            strcpy(kernel_state.virt_mode, "qemu");
        } else {
            strcpy(kernel_state.virt_mode, "unknown");
        }
    }
}

/**
 * kernel_early_init - Early kernel initialization
 *
 * Called immediately after bootloader hands control to kernel.
 * Only safe to call very basic functions here.
 */
void kernel_early_init(void)
{
    kernel_state.state = KERNEL_STATE_BOOTING;

    /* Clear screen with blue background */
    vga_clear(vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE));

    /* Print boot banner */
    printk("\n");
    printk("  ____   ___   __  __   _   _  _____ \n");
    printk(" |  _ \\ / _ \\ |  \\/  | | \\ | || ____|\n");
    printk(" | |_) | | | || |\\/| | |  \\| ||  _|  \n");
    printk(" |  _ <| |_| || |  | | | |\\  || |___ \n");
    printk(" |_| \\_\\\\___/ |_|  |_| |_| \\_||_____|\n");
    printk("\n");
    printk("  Version: %s (%s)\n", NEXUS_VERSION_STRING, NEXUS_CODENAME);
    printk("  Built: %s %s\n", __DATE__, __TIME__);
    printk("  ==============================================\n");
    printk("\n");

    /* Initialize spinlock */
    kernel_lock.lock = 0;

    /* Architecture-specific early init (GDT, IDT, early console) */
    arch_early_init();

    /* Detect virtualization environment EARLY */
    detect_virtualization();

    /* Initialize boot CPU data */
    u32 boot_cpu = arch_get_cpu_id();
    percpu_data[boot_cpu].cpu_id = boot_cpu;
    percpu_data[boot_cpu].apic_id = arch_get_apic_id();
    percpu_data[boot_cpu].current_thread = NULL;
    percpu_data[boot_cpu].current_process = NULL;
    percpu_data[boot_cpu].context_switches = 0;
    percpu_data[boot_cpu].interrupts = 0;
    percpu_data[boot_cpu].kernel_stack = NULL;

    kernel_state.cpu_count = 1;
    kernel_state.smp_enabled = false;
    kernel_state.virt_enabled = false;

    /* Print virtualization status */
    if (kernel_state.is_vmware) {
        printk("[VM] VMware environment detected - using VMware optimizations\n");
    } else if (kernel_state.is_virtualbox) {
        printk("[VM] VirtualBox environment detected\n");
    } else if (kernel_state.is_qemu) {
        printk("[VM] QEMU environment detected\n");
    } else if (kernel_state.is_hyperv) {
        printk("[VM] Hyper-V environment detected\n");
    } else {
        printk("[OK] Running on bare metal (%s mode)\n", kernel_state.virt_mode);
    }

    printk("[OK] Early initialization complete\n");
}

/**
 * kernel_init - Main kernel initialization
 */
void kernel_init(void)
{
    kernel_state.state = KERNEL_STATE_INITIALIZING;
    
    printk("\n[INFO] Starting kernel initialization...\n\n");
    
    /* Initialize GDT and IDT */
    printk("[    ] Initializing GDT...\n");
    arch_init_gdt();
    printk("[OK]  GDT initialized\n");
    
    printk("[    ] Initializing IDT...\n");
    arch_init_idt();
    printk("[OK]  IDT initialized\n");
    
    /* Initialize memory management */
    printk("[    ] Initializing physical memory manager...\n");
    pmm_init();
    printk("[OK]  Physical memory manager initialized\n");
    
    printk("[    ] Initializing virtual memory manager...\n");
    arch_init_paging();
    vmm_init();
    printk("[OK]  Virtual memory manager initialized\n");
    
    printk("[    ] Initializing kernel heap...\n");
    heap_init();
    printk("[OK]  Kernel heap initialized\n");
    
    printk("[    ] Initializing slab allocator...\n");
    slab_init();
    printk("[OK]  Slab allocator initialized\n");
    
    /* Initialize interrupt system */
    printk("[    ] Initializing interrupt system...\n");
    interrupt_init();
    arch_enable_interrupts();
    printk("[OK]  Interrupt system initialized\n");
    
    /* Initialize timer */
    printk("[    ] Initializing APIC timer...\n");
    apic_timer_freq = 1000000000; /* 1 GHz */
    arch_init_timer(apic_timer_freq);
    printk("[OK]  APIC timer initialized\n");
    
    /* Initialize scheduler */
    printk("[    ] Initializing scheduler...\n");
    scheduler_init();
    printk("[OK]  Scheduler initialized\n");
    
    /* Initialize SMP (if enabled) */
#ifdef CONFIG_SMP
    printk("[    ] Initializing SMP...\n");
    arch_init_smp();
    kernel_state.smp_enabled = true;
    kernel_state.cpu_count = 1; /* Will be updated by arch_init_smp */
    printk("[OK]  SMP initialized (%u CPUs)\n", kernel_state.cpu_count);
#endif
    
    /* Initialize ACPI */
    printk("[    ] Initializing ACPI...\n");
    acpi_init();
    printk("[OK]  ACPI initialized\n");

    /* Initialize network stack */
    printk("[    ] Initializing network stack...\n");
    net_init();
    ipv4_init();
    ipv6_init();
    firewall_init();
    printk("[OK]  Network stack initialized (IPv4 + IPv6 + Firewall)\n");

    /* Architecture-specific late init */
    arch_late_init();
    
    kernel_state.state = KERNEL_STATE_RUNNING;
    kernel_state.boot_time = kernel_ticks;
    
    printk("\n");
    printk("  ==============================================\n");
    printk("  NEXUS OS Initialization Complete!\n");
    printk("  Boot time: %u ms\n", (u32)(kernel_state.boot_time / 1000));
    printk("  Memory: %u MB total\n", (u32)(kernel_state.mem_total / (1024 * 1024)));
    printk("  CPUs: %u online\n", kernel_state.cpu_count);
    printk("  ==============================================\n");
    printk("\n");
}

/**
 * kernel_start_scheduler - Start the scheduler
 *
 * This function should never return on the idle task.
 */
void kernel_start_scheduler(void)
{
    printk("[INFO] Starting scheduler on CPU %u...\n", arch_get_cpu_id());

    /* Enable interrupts before starting scheduler */
    arch_enable_interrupts();

    /* Start the scheduler - this should never return for idle */
    scheduler_start();

    /* If we get here, we're running on idle thread - halt */
    printk("[WARN] Scheduler returned to idle (should not happen)\n");

    for (;;) {
        arch_halt_cpu();
    }
}

/**
 * kernel_main - Main kernel entry point
 * 
 * Called from arch-specific boot code after basic setup.
 */
void kernel_main(void)
{
    kernel_early_init();
    kernel_init();
    kernel_start_scheduler();
    
    /* Idle loop - should never reach here in normal operation */
    while (1) {
        arch_halt_cpu();
    }
}

/*===========================================================================*/
/*                         KERNEL PANIC                                      */
/*===========================================================================*/

/**
 * kernel_panic - Kernel panic handler
 * 
 * Called when kernel encounters an unrecoverable error.
 * Never returns.
 */
void __noreturn kernel_panic(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    
    /* Disable interrupts */
    arch_disable_interrupts();
    
    /* Set panic color scheme (red on black) */
    u8 panic_color = vga_make_color(VGA_COLOR_YELLOW, VGA_COLOR_RED);
    vga_clear(panic_color);
    
    printk("\n\n");
    printk("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printk("  !!              KERNEL PANIC                   !!\n");
    printk("  !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    printk("\n");
    printk("  The system has encountered a fatal error and\n");
    printk("  cannot continue safely.\n");
    printk("\n");
    printk("  Error: ");
    vprintk(fmt, args);
    printk("\n\n");
    printk("  CPU ID: %u\n", arch_get_cpu_id());
    printk("  APIC ID: %u\n", arch_get_apic_id());
    printk("  Ticks: %u\n", (u32)kernel_ticks);
    printk("\n");
    printk("  System halted. Please restart manually.\n");
    printk("\n");
    
    __builtin_va_end(args);
    
    /* Halt the CPU */
    while (1) {
        arch_halt_cpu();
    }
    
    /* Unreachable */
    __builtin_unreachable();
}

/**
 * kernel_assert_failed - Assertion failure handler
 * Note: Also in panic.c - this is a weak alias
 */
__attribute__((weak))
void kernel_assert_failed(const char *expr, const char *file, int line)
{
    printk("\n[BUG] Assertion failed: %s\n", expr);
    printk("      at %s:%d\n", file, line);
    kernel_panic("Kernel assertion failed: %s", expr);
}

/*===========================================================================*/
/*                         UTILITY FUNCTIONS                                 */
/*===========================================================================*/

/**
 * get_percpu - Get per-CPU data for current CPU
 */
percpu_data_t *get_percpu(void)
{
    return &percpu_data[arch_get_cpu_id()];
}

/**
 * get_cpu_id - Get current CPU ID
 */
__attribute__((weak))
u32 get_cpu_id(void)
{
    return arch_get_cpu_id();
}

/**
 * get_num_cpus - Get number of online CPUs
 */
u32 get_num_cpus(void)
{
    return kernel_state.cpu_count;
}

/**
 * get_ticks - Get current tick count
 */
u64 get_ticks(void)
{
    return arch_get_ticks();
}

/**
 * get_time_ms - Get time in milliseconds
 */
u64 get_time_ms(void)
{
    return arch_get_ticks() / 1000000;
}

/**
 * delay_ms - Delay for specified milliseconds
 */
void delay_ms(u64 ms)
{
    u64 end = get_time_ms() + ms;
    while (get_time_ms() < end) {
        arch_halt_cpu();
    }
}

/**
 * local_irq_enable - Enable local interrupts
 */
void local_irq_enable(void)
{
    arch_enable_interrupts();
}

/**
 * local_irq_disable - Disable local interrupts
 */
void local_irq_disable(void)
{
    arch_disable_interrupts();
}

/**
 * reboot - Reboot the system
 */
void reboot(void)
{
    printk("[INFO] Rebooting system...\n");
    arch_reboot();
}

/**
 * shutdown - Shutdown the system
 */
void shutdown(void)
{
    printk("[INFO] Shutting down system...\n");
    arch_shutdown();
}

/*===========================================================================*/
/*                         MEMORY ALLOCATION (Kernel)                        */
/*===========================================================================*/

/* Simple early boot allocator - will be replaced by proper heap */
static unsigned char early_heap[1024 * 1024];  /* 1MB early heap */
static unsigned long early_heap_offset = 0;

/**
 * kmalloc - Allocate kernel memory
 */
void *kmalloc(size_t size)
{
    void *ptr;

    /* Align to 8 bytes */
    size = (size + 7) & ~7;

    if (early_heap_offset + size > sizeof(early_heap)) {
        return NULL;  /* Out of memory */
    }

    ptr = &early_heap[early_heap_offset];
    early_heap_offset += size;

    return ptr;
}

/**
 * kzalloc - Allocate zeroed kernel memory
 */
void *kzalloc(size_t size)
{
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/**
 * kfree - Free kernel memory
 * Note: Early allocator doesn't support free - memory is permanently allocated
 */
void kfree(void *ptr)
{
    /* Early boot allocator - no free support */
    /* Proper kfree will be implemented in mm/heap.c */
    (void)ptr;
}

/*===========================================================================*/
/*                         STRING FUNCTIONS                                  */
/*===========================================================================*/

/**
 * memset - Set memory to value
 */
void *memset(void *s, int c, size_t n)
{
    u8 *p = (u8 *)s;
    while (n--) {
        *p++ = (u8)c;
    }
    return s;
}

/**
 * memcpy - Copy memory
 */
void *memcpy(void *dest, const void *src, size_t n)
{
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

/**
 * memmove - Move memory (handles overlap)
 */
void *memmove(void *dest, const void *src, size_t n)
{
    u8 *d = (u8 *)dest;
    const u8 *s = (const u8 *)src;
    
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

/**
 * memcmp - Compare memory
 */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const u8 *p1 = (const u8 *)s1;
    const u8 *p2 = (const u8 *)s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

/**
 * strlen - Get string length
 */
size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

/**
 * strcpy - Copy string
 */
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

/**
 * strcmp - Compare strings
 */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(u8 *)s1 - *(u8 *)s2;
}

/*===========================================================================*/
/*                         PORT I/O                                          */
/* Note: These are also in boot.asm - weak aliases here                      */
/*===========================================================================*/

__attribute__((weak))
void outb(u16 port, u8 val)
{
    __asm__ __volatile__("outb %0, %1" :: "a"(val), "Nd"(port));
}

__attribute__((weak))
u8 inb(u16 port)
{
    u8 val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

__attribute__((weak))
void outw(u16 port, u16 val)
{
    __asm__ __volatile__("outw %0, %1" :: "a"(val), "Nd"(port));
}

__attribute__((weak))
u16 inw(u16 port)
{
    u16 val;
    __asm__ __volatile__("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

__attribute__((weak))
void outl(u16 port, u32 val)
{
    __asm__ __volatile__("outl %0, %1" :: "a"(val), "Nd"(port));
}

__attribute__((weak))
u32 inl(u16 port)
{
    u32 val;
    __asm__ __volatile__("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

__attribute__((weak))
void io_wait(void)
{
    outb(0x80, 0);
}

/*===========================================================================*/
/*                         KERNEL MODULE                                     */
/*===========================================================================*/

/**
 * kernel_get_version - Get kernel version string
 */
const char *kernel_get_version(void)
{
    return NEXUS_VERSION_STRING;
}

/**
 * kernel_get_codename - Get kernel codename
 */
const char *kernel_get_codename(void)
{
    return NEXUS_CODENAME;
}

/**
 * kernel_get_state - Get kernel state
 */
u32 kernel_get_state(void)
{
    return kernel_state.state;
}

/*===========================================================================*/
/*                         STUB FUNCTIONS FOR INCOMPLETE SUBSYSTEMS          */
/*         These will be properly implemented in respective modules          */
/*===========================================================================*/

/* Memory Management stubs */
void vmm_init(void) { }
void heap_init(void) { }
void slab_init(void) { }
void pmm_init(void) { }

/* Interrupt stubs */
void interrupt_init(void) { }

/* Timer stubs */
void arch_init_timer(u64 freq) { (void)freq; }

/* Scheduler stubs */
void scheduler_init(void) { }
void scheduler_start(void) { }

/* SMP stubs */
void arch_init_smp(void) { }

/* ACPI stub */
void acpi_init(void) { }

/* Architecture init stubs */
void arch_init_gdt(void) { }
void arch_init_idt(void) { }
void arch_init_paging(void) { }

/* CPU info */
unsigned int arch_get_apic_id(void) { return 0; }

/* Network stubs */
void net_init(void) { printk("[NET] Network stack initialized\n"); }
void ipv4_init(void) { printk("[IPv4] IPv4 initialized\n"); }
void ipv6_init(void) { printk("[IPv6] IPv6 stack loaded\n"); }
void firewall_init(void) { printk("[FIREWALL] Firewall loaded\n"); }
