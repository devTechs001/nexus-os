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
#include "../include/userspace.h"
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
/*
 * COM1 mirror.
 *
 * Everything the kernel prints went ONLY to the VGA text buffer at 0xB8000.
 * Once GRUB has put the machine into a linear framebuffer graphics mode that
 * memory is no longer scanned out, so every diagnostic - including
 * "[SPLASH] ... skipped", the framebuffer address, and the probe result - was
 * written somewhere nobody could ever see. A blank screen and a working boot
 * produced byte-identical evidence.
 *
 * Mirroring to COM1 (0x3F8) makes the boot log readable over a serial console
 * regardless of what the display is doing, which is what any "the screen is
 * blank" report actually needs.
 */
static void serial_mirror(char c)
{
    /* Wait for the transmit holding register to drain (bounded). */
    for (u32 spin = 0; spin < 100000; spin++) {
        if (inb(0x3FD) & 0x20) {
            break;
        }
    }
    outb(0x3F8, (u8)c);
}

static void vga_putchar(char c, u8 color)
{
    serial_mirror(c);

    /*
     * Keep the recent log in RAM so it can be painted onto a linear
     * framebuffer when the VGA text buffer is not being scanned out. Without
     * this the only record of a failed graphical boot is invisible.
     */
    extern void fbcon_capture_char(char c);
    fbcon_capture_char(c);

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
void vga_clear(u8 color)
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
                /*
                 * Read as int, NOT s64.
                 *
                 * Varargs are promoted to int and no further, so a caller
                 * passing -5 puts a 32-bit -5 in a register. Consuming 64 bits
                 * for it read the (zero-extended) full register and printed
                 * 4294967291. Every negative %d in the tree - hundreds of
                 * error codes, return values and deltas - printed as a huge
                 * unsigned number. Verified against the real SysV ABI.
                 */
                int val = __builtin_va_arg(args, int);
                if (val < 0) {
                    vga_putchar('-', color);
                    (void)val;
                    print_decimal((u64)(-(s64)val), color);
                } else {
                    print_decimal((u64)val, color);
                }
                break;
            }
            case 'u': {
                /* 32-bit read: matches how the call sites pass values.
                   Use %lu / %llu for 64-bit ones. */
                unsigned int val = __builtin_va_arg(args, unsigned int);
                print_decimal((u64)val, color);
                break;
            }
            case 'x': {
                unsigned int val = __builtin_va_arg(args, unsigned int);
                print_hex((u64)val, color, false, 0);
                break;
            }
            case 'X': {
                unsigned int val = __builtin_va_arg(args, unsigned int);
                print_hex((u64)val, color, true, 0);
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
                /*
                 * Length modifier. Values are promoted to 64-bit by default
                 * here, so consume "l" and any second "l" and then decode
                 * the real conversion.
                 *
                 * The previous code did `p++; continue;`, which skipped the
                 * 'l' without consuming the argument, then printed the
                 * conversion letter literally on the next iteration. Varargs
                 * are positional, so every %lu/%llu/%llX call read one
                 * argument too few and every following value in that call was
                 * the wrong one - 450+ call sites across the tree.
                 */
                p++;
                if (*p == 'l') {
                    p++;
                }
                switch (*p) {
                case 'd':
                case 'i': {
                    /* %ld / %lld: callers pass long or u64-width values. */
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
                default:
                    vga_putchar('%', color);
                    vga_putchar('l', color);
                    vga_putchar(*p ? *p : '?', color);
                    break;
                }
                break;
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
    } else if (strcmp(hypervisor_sig, "KVMKVMKVM") == 0 ||
               strncmp(hypervisor_sig, "TCGTCGTCG", 9) == 0) {
        /*
         * QEMU reports "KVMKVMKVM" under KVM and "TCGTCGTCGTCG" without it
         * (pure software emulation). The TCG case used to fall through to the
         * CPUID-vendor fallback below and end up as "native", so is_qemu was
         * never set in the configuration "make run" actually uses.
         */
        kernel_state.is_qemu = true;
        strcpy(kernel_state.virt_mode, "qemu");
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

    /*
     * scheduler_start() initializes the run queue and returns; the boot thread
     * is not a task, so nothing would ever switch away from it. Enter the
     * scheduler explicitly.
     *
     * This used to fall straight into `arch_halt_cpu()`, which is `cli; hlt` -
     * it stopped the CPU with interrupts disabled, so the timer could not even
     * fire, scheduler_tick() was never called, and no task ever ran. The GUI
     * task, the init task and the service manager were all queued and none of
     * them ever got a timeslice.
     *
     * scheduler_enter_from_boot() makes the boot thread the current task and
     * switches into the idle task, which halts with interrupts enabled and
     * picks up whatever is runnable.
     */
    printk("[INFO] Entering the scheduler\n");
    extern void scheduler_enter_from_boot(void);
    scheduler_enter_from_boot();

    /*
     * Reaching here means there was no runnable task and no valid context to
     * resume. Idle with interrupts enabled so the system keeps ticking rather
     * than locking up with interrupts off.
     */
    printk("[WARN] Scheduler returned to the boot thread\n");
    for (;;) {
        __asm__ __volatile__("sti; hlt");
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

/* Global boot parameters */
extern boot_params_t g_boot_params;

/* Global buffer for boot menu command line */
__section(".bss") char g_boot_cmdline[512];

/*
 * The command line actually in force for this boot: either the GRUB-provided
 * one or whatever the interactive boot menu selected. Userspace init reads this
 * through SYS_GET_CMDLINE, so it must outlive kernel_main().
 */
__section(".bss") char g_boot_cmdline_effective[BOOT_CMDLINE_MAX];

/*
 * The userspace init image, as delivered by the bootloader.
 *
 * Captured from the multiboot2 module tag during kernel_main(). The kernel does
 * not copy it: GRUB places modules in low memory that stays mapped and
 * untouched, and the ELF loader copies the segments it needs into its own
 * allocation. These are plain physical addresses the tag supplied.
 */
u64  g_userspace_module_start;
u64  g_userspace_module_end;
char g_userspace_module_name[USERSPACE_MODULE_NAME_MAX];
bool g_userspace_module_present;

/**
 * boot_cmdline_set_effective - Record the command line userspace should see
 * @src: NUL-terminated option string to copy (may be NULL)
 *
 * Bounded copy: the bootloader's command line is attacker-controlled input and
 * may be longer than the buffer.
 */
void boot_cmdline_set_effective(const char *src)
{
    const char *cmdline = (const char *)src;

    if (!cmdline) {
        g_boot_cmdline_effective[0] = '\0';
        return;
    }

    for (u32 i = 0; i < sizeof(g_boot_cmdline_effective) - 1; i++) {
        g_boot_cmdline_effective[i] = cmdline[i];
        if (cmdline[i] == '\0') {
            return;
        }
    }

    g_boot_cmdline_effective[sizeof(g_boot_cmdline_effective) - 1] = '\0';
}

/* External boot menu entry point */
extern void boot_menu_main(void);

/**
 * kernel_main - Main kernel entry point
 * @multiboot_info: Pointer to multiboot info structure from bootloader
 *
 * Called from arch-specific boot code after basic setup.
 */
void kernel_main(u64 multiboot_info)
{
    const char *cmdline = NULL;

    /*
     * Initialize kernel state before anything reads it.
     *
     * BSS is zeroed by the boot stub, so cpu_count was left at 0 and
     * get_num_cpus() returned 0. scheduler_init() loops over
     * "cpu < get_num_cpus()", so it created *no* run queues and *no* idle
     * tasks. scheduler_start() then dereferenced idle_task(0) (NULL) and
     * wrote through it, faulting in the late stage of boot. Per-CPU data
     * was likewise never set up.
     */
    kernel_state.version = NEXUS_VERSION_STRING;
    kernel_state.codename = NEXUS_CODENAME;
    kernel_state.boot_time = 0;
    kernel_state.state = KERNEL_STATE_BOOTING;
    kernel_state.mem_total = 0;
    kernel_state.mem_used = 0;
    kernel_state.cpu_count = 1;      /* BSP; raised by SMP detection */
    kernel_state.smp_enabled = false;
    kernel_state.virt_enabled = false;

    /* Early serial debug - confirm we reached kernel_main */
    outb(0x3F8, 'K');
    outb(0x3F8, 'E');
    outb(0x3F8, 'R');
    outb(0x3F8, 'N');
    outb(0x3F8, 'E');
    outb(0x3F8, 'L');
    outb(0x3F8, '\n');

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
            }
            /* Type 8 = framebuffer info tag */
            else if (type == 8 && size >= 32) {
                g_boot_params.gfx_addr = *(u64 *)((u8 *)tag + 8);
                g_boot_params.gfx_pitch = tag[4];
                g_boot_params.gfx_width = tag[5];
                g_boot_params.gfx_height = tag[6];
                g_boot_params.gfx_depth = ((u8 *)tag)[28];
                g_boot_params.gfx_type = ((u8 *)tag)[29];
            }
            /*
             * Type 3 = module tag.
             *
             * Layout (multiboot2 spec, 8-byte tag header then payload):
             *   +0   u32 type      = 3
             *   +4   u32 size
             *   +8   u32 mod_start  physical start of the module
             *   +12  u32 mod_end    physical end (exclusive)
             *   +16  char[] string  the module's command line / file name
             *
             * The userspace init image arrives this way: it is packaged into
             * the ISO as a GRUB "module" directive and is the only way it can
             * reach the kernel, because no root filesystem is mounted at boot
             * and the VFS-backed ELF loader has nothing to read from.
             *
             * Modules are handed to us in the order they appear in grub.cfg.
             * The first one is PID 1; any further modules are ignored for now.
             */
            else if (type == 3 && size >= 16) {
                if (!g_userspace_module_present) {
                    g_userspace_module_start = tag[2];
                    g_userspace_module_end = tag[3];

                    /*
                     * The name follows the fixed fields and may be a
                     * zero-length string; bound it so a malformed tag cannot
                     * make us read past the tag's own size.
                     */
                    const char *mod_name = (const char *)((u8 *)tag + 16);
                    u32 name_space = size - 16;
                    u32 n = 0;

                    while (n + 1 < sizeof(g_userspace_module_name) &&
                           n < name_space && mod_name[n]) {
                        g_userspace_module_name[n] = mod_name[n];
                        n++;
                    }
                    g_userspace_module_name[n] = '\0';

                    g_userspace_module_present = true;

                    printk("[BOOT] Userspace module: %llu bytes at 0x%llX %s\n",
                           (unsigned long long)(g_userspace_module_end -
                                                g_userspace_module_start),
                           (unsigned long long)g_userspace_module_start,
                           g_userspace_module_name);
                }
            }

            /* Move to next tag (align to 8 bytes) */
            offset += (size + 7) & ~7;
        }
    }

    /*
     * Parse boot parameters or show menu.
     *
     * Whichever string ends up in force is also copied into
     * g_boot_cmdline_effective. Userspace init reads it through
     * SYS_GET_CMDLINE and parses the same options the kernel did, which it
     * could not do before: init_get_cmdline() was declared and called in
     * userspace/init/init.c but defined nowhere, so the option string never
     * reached PID 1 and every init option was ignored.
     */
    if (cmdline && cmdline[0]) {
        parse_boot_params(cmdline);
        boot_cmdline_set_effective(cmdline);
    } else {
        printk("[BOOT] No command line provided. Launching Interactive Boot Menu...\n");
        boot_menu_main();

        /* If boot_menu_main returns, it has filled g_boot_cmdline */
        if (g_boot_cmdline[0]) {
            printk("[BOOT] Booting with parameters: %s\n", g_boot_cmdline);
            parse_boot_params(g_boot_cmdline);
            boot_cmdline_set_effective(g_boot_cmdline);
        } else {
            printk("[BOOT] No selection made, using default VMware mode\n");
            parse_boot_params("quiet splash virt=vmware");
            boot_cmdline_set_effective("quiet splash virt=vmware");
        }
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

/*
 * TSC calibration.
 *
 * arch_get_ticks() returns the raw TSC cycle counter (see boot.asm: rdtsc).
 * get_time_ms() divided that by 1e6 as if it were nanoseconds, which made
 * the clock run at CPU-frequency/1e6 milliseconds per second - roughly 3000x
 * too fast on a 3 GHz part. Everything built on it was wrong: APIC timer
 * calibration measured almost no ticks (hence "Timer frequency: 0 Hz"),
 * delay_ms() returned immediately, and the boot splash could never animate.
 *
 * Calibrate once against the PIT so cycles can be converted to real time.
 */
static u64 g_tsc_hz = 0;

/**
 * tsc_calibrate - Measure the TSC frequency using the PIT as reference
 *
 * Uses PIT channel 2 (one-shot, gated by port 0x61) which does not disturb
 * the system tick. Falls back to a sane estimate if the PIT is unavailable,
 * so timing degrades rather than misbehaving.
 */
static u64 tsc_calibrate(void)
{
    u64 start, end;
    u32 guard;

    if (g_tsc_hz) {
        return g_tsc_hz;
    }

    /* PIT channel 2, mode 0 (interrupt on terminal count), 1193182 Hz base */
    outb(0x61, (inb(0x61) & 0xFD) | 0x01);   /* gate on, speaker off */
    outb(0x43, 0xB0);                        /* ch2, lo/hi, mode 0, binary */
    u32 reload = 11932;                      /* ~10 ms at 1.193182 MHz */
    outb(0x42, (u8)(reload & 0xFF));
    outb(0x42, (u8)(reload >> 8));

    /* Restart the counter cleanly */
    u8 p61 = inb(0x61) & 0xFE;
    outb(0x61, p61);
    outb(0x61, p61 | 0x01);

    start = arch_get_ticks();

    /* Wait for the PIT output bit (0x20) to go high: 10 ms elapsed */
    guard = 0;
    while (!(inb(0x61) & 0x20)) {
        if (++guard > 100000000u) {
            break;   /* PIT not counting; fall through to the estimate */
        }
    }

    end = arch_get_ticks();
    outb(0x61, inb(0x61) & 0xFC);            /* gate off */

    u64 cycles = end - start;

    if (cycles > 1000 && guard < 100000000u) {
        /* cycles elapsed in ~10 ms -> multiply by 100 for cycles/second */
        g_tsc_hz = cycles * 100;
    } else {
        /*
         * Without a working PIT reference, assume 1 GHz. Timing will be off
         * on real hardware but monotonic and roughly sane, which is far
         * better than a clock that jumps ~3000x.
         */
        g_tsc_hz = 1000000000ULL;
        printk("[TIME] TSC calibration unavailable, assuming %llu Hz\n",
               (unsigned long long)g_tsc_hz);
        return g_tsc_hz;
    }

    printk("[TIME] TSC frequency: %llu Hz\n", (unsigned long long)g_tsc_hz);
    return g_tsc_hz;
}

u64 get_tsc_frequency(void)
{
    return tsc_calibrate();
}

/**
 * get_time_ns - Get monotonic time in nanoseconds
 */
u64 get_time_ns(void)
{
    u64 hz = tsc_calibrate();
    u64 now;

    if (!hz) {
        return 0;
    }

    /*
     * cycles * 1e9 / hz, split so the intermediate product cannot overflow:
     * a raw TSC value near 2^64 times 1e9 would wrap.
     */
    now = arch_get_ticks();
    return (now / hz) * 1000000000ULL +
           ((now % hz) * 1000000000ULL) / hz;
}

/**
 * get_time_ms - Get time in milliseconds
 */
u64 get_time_ms(void)
{
    return get_time_ns() / 1000000;
}

/**
 * delay_ms - Delay for specified milliseconds
 */
void delay_ms(u64 ms)
{
    u64 start = get_time_ms();

    /*
     * Busy-wait from here on.
     *
     * This loop used to call arch_halt_cpu(), which is "cli; hlt". Halting
     * with interrupts disabled means nothing can ever wake the CPU, so any
     * delay_ms() call hung the machine permanently - one of the reasons
     * boot appeared to stall in the late stage. get_time_ms() is derived
     * from the TSC, so a busy-wait needs no interrupts at all.
     */
    while ((get_time_ms() - start) < ms) {
        /* clock went backwards / TSC unusable - bail out rather than hang */
        if (get_time_ms() < start) {
            break;
        }
        __asm__ __volatile__("pause" ::: "memory");
    }
}

/**
 * mdelay - Delay for specified milliseconds (Linux-style alias)
 *
 * drivers/video/dsi.c calls this during panel power sequencing; it was
 * never defined, so the file relied on an implicit declaration.
 */
void mdelay(u32 ms)
{
    delay_ms((u64)ms);
}

/**
 * udelay - Delay for specified microseconds
 */
void udelay(u32 us)
{
    /* delay_ms() cannot express sub-millisecond waits; round up to 1 ms */
    delay_ms(us >= 1000 ? (u64)(us / 1000) : 1);
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

/*
 * The kernel heap lives in kernel/mm/heap.c and owns early-boot allocation
 * (early_alloc()). The 1 MB static scratch array that used to sit here, plus
 * the commented-out kmalloc/kzalloc/kfree copies that consumed it, were dead:
 * nothing referenced either, and keeping a second 1 MB .bss arena around only
 * shadowed the real allocator.
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

/* Memory Management - now implemented in kernel/mm/ (mm.h) */

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
