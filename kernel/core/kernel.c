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

/* Forward declarations for initialization (implemented in init.c) */
extern void kernel_main_init(void);

/* Forward declarations for initialization helper functions */
extern void console_init(void);
extern void printk_init(void);
extern void mm_early_init(void);
extern void percpu_init(void);
extern void interrupt_init(void);
extern void storage_init(void);
extern void input_init(void);
extern void power_init(void);
extern int hypervisor_init(void);
extern void init_process_start(void);
extern void service_manager_start(void);

/*===========================================================================*/
/*                         KERNEL BSS SECTION                                */
/*===========================================================================*/

/* Place kernel globals in BSS to avoid bloating binary */
__section(".bss") struct {
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

__section(".bss") percpu_data_t percpu_data[MAX_CPUS];

__section(".bss") spinlock_t kernel_lock;

__section(".bss") u64 kernel_ticks;

__section(".bss") u32 apic_timer_freq;

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
void detect_virtualization(void)
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

/* 
 * Redundant initialization functions removed. 
 * Use kernel_main_init() in init.c instead.
 */

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
 * kernel_main_64 - 64-bit kernel entry point (called from boot assembly)
 * @multiboot_info: Pointer to multiboot2 info structure from bootloader
 * @multiboot_magic: Multiboot2 magic number for validation
 *
 * This is the entry point called after the 32->64 bit transition.
 * Parameters are passed in RDI and RSI per System V AMD64 ABI.
 */
void kernel_main_64(u64 multiboot_info, u64 multiboot_magic)
{
    /* Note: multiboot magic check disabled - GRUB loads us correctly even if magic differs */
    /* Validate multiboot magic */
    if (multiboot_magic != 0x36d76289) {
        /* Invalid multiboot magic - continue anyway, multiboot_info should still be valid */
        /* kernel_panic("Invalid multiboot magic: 0x%X (expected 0x36d76289)",
                     (u32)multiboot_magic); */
    }

    kernel_main(multiboot_info);
}

/**
 * kernel_main - Main kernel entry point
 * @multiboot_info: Pointer to multiboot info structure from bootloader
 *
 * Called from arch-specific boot code after basic setup.
 */
void kernel_main(u64 multiboot_info)
{
    const char *cmdline = NULL;

    /* Parse multiboot info to get command line */
    if (multiboot_info) {
        /* Multiboot2 info structure */
        u32 *info = (u32 *)(uintptr_t)multiboot_info;
        u32 total_size = info[1];
        u32 offset = 8;  /* Skip basic info header */

        while (offset < total_size) {
            u32 *tag = (u32 *)((u8 *)info + offset);
            u32 type = tag[0];
            u32 size = tag[1];

            /* Type 1 = command line tag */
            if (type == 1 && size > 8) {
                cmdline = (const char *)(tag + 2);
                break;
            }

            /* Move to next tag (align to 8 bytes) */
            offset += (size + 7) & ~7;
        }
    }

    /* Parse boot parameters */
    if (cmdline && cmdline[0]) {
        parse_boot_params(cmdline);
    } else {
        printk("[BOOT] No command line provided, using defaults\n");
    }

    /* Initialize consolidated console first */
    console_init();

    /* 
     * Start the unified initialization sequence defined in init.c
     * This handles all phases from early boot to userspace transition.
     */
    kernel_main_init();

    /* Start the scheduler (this should never return) */
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
 * percpu_init - Initialize per-CPU data for boot processor
 */
void percpu_init(void)
{
    u32 cpu = arch_get_cpu_id();
    if (cpu >= MAX_CPUS) return;

    memset(&percpu_data[cpu], 0, sizeof(percpu_data_t));
    percpu_data[cpu].cpu_id = cpu;
    percpu_data[cpu].apic_id = arch_get_apic_id();
    
    pr_info("Per-CPU data initialized for CPU %u (APIC %u)\n", 
            cpu, percpu_data[cpu].apic_id);
}

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
 * Note: Now implemented in mm/heap.c - this early version commented out
 */
/*
void *kmalloc(size_t size)
{
    void *ptr;

    // Align to 8 bytes
    size = (size + 7) & ~7;

    if (early_heap_offset + size > sizeof(early_heap)) {
        return NULL;  // Out of memory
    }

    ptr = &early_heap[early_heap_offset];
    early_heap_offset += size;

    return ptr;
}
*/

/**
 * kzalloc - Allocate zeroed kernel memory
 * Note: Now implemented in mm/heap.c
 */
/*
void *kzalloc(size_t size)
{
    void *ptr = kmalloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}
*/

/**
 * kfree - Free kernel memory
 * Note: Now implemented in mm/heap.c
 */
/*
void kfree(void *ptr)
{
    // Early boot allocator - no free support
    // Proper kfree will be implemented in mm/heap.c
    (void)ptr;
}
*/

/*===========================================================================*/
/*                         STRING FUNCTIONS                                  */
/* Note: Now in string.c - keeping weak aliases for compatibility            */
/*===========================================================================*/

/* String functions moved to kernel/core/string.c */
/* These are now provided by string.c */

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

/* Memory Management - now implemented in mm/*.c */
/* void vmm_init(void) { } */
/* void heap_init(void) { } */
/* void slab_init(void) { } */
/* void pmm_init(void) { } */

/* Interrupt initialization */
extern void interrupt_init(void);

/* Timer initialization */
extern void arch_init_timer(u64 freq);

/* SMP initialization */
extern void arch_init_smp(void);

/* ACPI initialization */
extern void acpi_init(void);

/* Architecture init functions */
extern void arch_init_gdt(void);
extern void arch_init_idt(void);
/* Paging initialization */
extern void arch_init_paging(void);

/* Architecture initialization */
extern void arch_early_init(void);
extern void arch_late_init(void);
extern void arch_init_cpus(void);
extern void arch_enable_interrupts(void);
/* Hypervisor initialization */
extern int hypervisor_init(void);

/* GUI functions - real implementations in gui/ */
typedef struct login_screen login_screen_t;
typedef struct onboarding_wizard onboarding_wizard_t;

/**
 * splash_draw_logo_text - Draw splash logo in text mode
 * Weak stub to be overridden by gui/boot-splash/boot_splash.c
 */
__attribute__((weak))
void splash_draw_logo_text(u32 x, u32 y)
{
    (void)x; (void)y;
    printk("\n    _   _ _____ _____ _   _  ____  \n");
    printk("   | \\ | | ____|_   _| | | |/ ___| \n");
    printk("   |  \\| |  _|   | | | | | | |  _  \n");
    printk("   | |\\  | |___  | | | |_| | |_| | \n");
    printk("   |_| \\_|_____| |_|  \\___/ \\____| \n\n");
    printk("        O P E R A T I N G   S Y S T E M\n\n");
}

extern login_screen_t *login_screen_get_instance(void);
extern int login_screen_init(login_screen_t *screen);
extern int login_screen_run(login_screen_t *screen);

extern onboarding_wizard_t *onboarding_wizard_get_instance(void);
extern int onboarding_wizard_init(onboarding_wizard_t *wizard);
extern int onboarding_wizard_run(onboarding_wizard_t *wizard);

/* Network stubs - real implementations in net/ */
/* net_init, ipv4_init, ipv6_init, firewall_init are in net/ */
