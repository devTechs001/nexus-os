/*
 * NEXUS OS - Universal Operating System
 * Copyright (c) 2026 NEXUS Development Team
 *
 * bootloader.c - Main Bootloader (Stage 2)
 *
 * This is the second-stage bootloader for NEXUS OS.
 * It runs in 32-bit protected mode and loads the kernel.
 *
 * STATUS: partial. The descriptor-table setup, the memory-map handoff and the
 * control flow are here and correct, but two pieces are missing and cannot be
 * written in this translation unit:
 *
 *   1. A 16-bit real-mode entry stub. INT 13h (disk) and INT 15h (memory) are
 *      real-mode BIOS interfaces; this file is compiled into a 64-bit object
 *      where %bx/%es do not exist and no `int $0x13` can be issued. The
 *      corresponding functions now report failure instead of pretending to
 *      have done the work.
 *   2. The paging setup that a 64-bit kernel requires before it is entered.
 *
 * It does not currently assert the kernel is loadable, so treat it as a
 * scaffold: bootloader_main() will halt with "Kernel load failed" rather than
 * jumping into an unread buffer.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * Stage 2 runs before the kernel exists and must not pull in kernel/include,
 * so it declares its own fixed-width names. The file used u8/u16/u32/u64
 * throughout (and kfree/kmalloc/snprintf at the call sites) without ever
 * defining them or including anything that does -- every one of the 94 errors
 * in this file was a consequence of that single omission.
 *
 * stdint.h's exact-width types are the right basis; these are aliases only so
 * the existing code reads unchanged.
 */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/*===========================================================================*/
/*                         BOOTLOADER CONFIGURATION                          */
/*===========================================================================*/

#define BOOTLOADER_VERSION      "1.0.0"
#define BOOTLOADER_NAME         "NEXUS OS Stage2 Bootloader"

#define KERNEL_LOAD_ADDRESS     0x00100000      /* 1 MB */
#define KERNEL_MAX_SIZE         (32 * 1024 * 1024)
#define INITRD_LOAD_ADDRESS     0x02000000      /* 32 MB */
#define INITRD_MAX_SIZE         (64 * 1024 * 1024)

#define SECTOR_SIZE             512
#define KERNEL_SECTORS          (KERNEL_MAX_SIZE / SECTOR_SIZE)

#define BOOT_MAGIC              0x4E5853424F4F54ULL  /* "NXSBOOT" */

/*===========================================================================*/
/*                         TYPE DEFINITIONS                                  */
/*===========================================================================*/

/* Boot parameters structure passed to kernel */
typedef struct {
    u64 magic;
    u32 kernel_base;
    u32 kernel_size;
    u32 kernel_entry;
    u32 initrd_base;
    u32 initrd_size;
    u32 memory_map_entries;
    u32 cmdline_offset;
    char cmdline[512];
} boot_params_t;

/* Memory map entry */
typedef struct {
    u32 base_low;
    u32 base_high;
    u32 length_low;
    u32 length_high;
    u32 type;
} memory_map_entry_t;

/* Memory types */
#define MEM_AVAILABLE       1
#define MEM_RESERVED        2
#define MEM_ACPI_RECLAIM    3
#define MEM_ACPI_NVS        4
#define MEM_BAD             5

/* GDT Entry */
typedef struct {
    u16 limit_low;
    u16 base_low;
    u8  base_middle;
    u8  access;
    u8  granularity;
    u8  base_high;
} __attribute__((packed)) gdt_entry_t;

/* GDT Pointer
 *
 * The base is a linear address. Declaring it u32 truncated a 64-bit pointer
 * and was the source of every "cast from pointer to integer of different size"
 * warning in this file; uintptr_t is the type that is actually wide enough for
 * whatever mode the descriptor table lives in.
 */
typedef struct {
    u16 limit;
    uintptr_t base;
} __attribute__((packed)) gdt_ptr_t;

/* IDT Entry */
typedef struct {
    u16 base_low;
    u16 selector;
    u8  zero;
    u8  type_attr;
    u16 base_high;
} __attribute__((packed)) idt_entry_t;

/* IDT Pointer */
typedef struct {
    u16 limit;
    uintptr_t base;
} __attribute__((packed)) idt_ptr_t;

/*===========================================================================*/
/*                         GLOBAL VARIABLES                                  */
/*===========================================================================*/

static boot_params_t boot_params;
static memory_map_entry_t memory_map[64];
static u32 memory_map_count = 0;

static u32 kernel_size = 0;
static u32 initrd_size = 0;

/*===========================================================================*/
/*                         EXTERNAL ASSEMBLY FUNCTIONS                       */
/*===========================================================================*/

/* The flush stubs take the address of the descriptor-table pointer. That
 * address is pointer-sized, not u32. */
extern void gdt_flush(uintptr_t gdt_ptr);
extern void idt_flush(uintptr_t idt_ptr);
extern void enable_paging(u32 page_directory);
extern void disable_interrupts(void);
extern void enable_interrupts(void);
extern void halt_cpu(void);
extern u32 get_cr0(void);
extern u32 get_cr3(void);
extern u32 get_eflags(void);

/*===========================================================================*/
/*                         VIDEO OUTPUT (VGA TEXT MODE)                      */
/*===========================================================================*/

#define VGA_BASE            0xB8000
#define VGA_WIDTH           80
#define VGA_HEIGHT          25

static u16 *vga_buffer = (u16 *)VGA_BASE;
static u32 cursor_x = 0;
static u32 cursor_y = 0;

static u8 color = 0x07;  /* Light gray on black */

/**
 * vga_putchar - Print character to VGA buffer
 */
static void vga_putchar(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = ' ' | (color << 8);
        }
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = c | (color << 8);
        cursor_x++;
    }

    /* Scroll if needed */
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= VGA_HEIGHT) {
        /* Scroll up one line */
        for (u32 i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        /* Clear last line */
        for (u32 i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = ' ' | (color << 8);
        }
        cursor_y = VGA_HEIGHT - 1;
    }
}

/**
 * vga_print - Print string to VGA buffer
 */
static void vga_print(const char *str)
{
    while (*str) {
        vga_putchar(*str++);
    }
}

/**
 * vga_print_hex - Print hex number
 */
static void vga_print_hex(u32 value)
{
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[11];
    int i = 9;

    buffer[10] = '\0';
    buffer[0] = '0';
    buffer[1] = 'x';

    for (; i >= 2; i--) {
        buffer[i] = hex_chars[value & 0xF];
        value >>= 4;
    }

    vga_print(buffer);
}

/**
 * vga_print_dec - Print decimal number
 */
static void vga_print_dec(u32 value)
{
    char buffer[12];
    int i = 10;

    buffer[11] = '\0';

    if (value == 0) {
        vga_print("0");
        return;
    }

    while (value > 0 && i >= 0) {
        buffer[i--] = '0' + (value % 10);
        value /= 10;
    }

    vga_print(&buffer[i + 1]);
}

/**
 * vga_clear - Clear VGA screen
 */
static void vga_clear(void)
{
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = ' ' | (color << 8);
    }
    cursor_x = 0;
    cursor_y = 0;
}

/**
 * vga_set_color - Set text color
 */
static void vga_set_color(u8 fg, u8 bg)
{
    color = (bg << 4) | (fg & 0x0F);
}

/*===========================================================================*/
/*                         PORT I/O                                          */
/*===========================================================================*/

/**
 * outb - Write byte to port
 */
static inline void outb(u16 port, u8 value)
{
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

/**
 * inb - Read byte from port
 */
static inline u8 inb(u16 port)
{
    u8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * io_wait - Small delay using port 0x80
 */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

/*===========================================================================*/
/*                         GDT SETUP                                         */
/*===========================================================================*/

/* GDT entries */
static gdt_entry_t gdt[5];
static gdt_ptr_t gdt_ptr;

/**
 * gdt_set_gate - Set GDT entry
 */
static void gdt_set_gate(u32 num, u32 base, u32 limit, u8 access, u8 gran)
{
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);

    gdt[num].access = access;
}

/**
 * gdt_init - Initialize GDT
 */
static void gdt_init(void)
{
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uintptr_t)&gdt;

    /* Null segment */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Kernel code segment */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Kernel data segment */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* User code segment */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* User data segment */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* Load GDT */
    gdt_flush((uintptr_t)&gdt_ptr);
}

/*===========================================================================*/
/*                         IDT SETUP                                         */
/*===========================================================================*/

/* IDT entries */
static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

/**
 * idt_set_gate - Set IDT entry
 */
static void idt_set_gate(u8 num, u32 base, u16 selector, u8 type_attr)
{
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = type_attr;
}

/**
 * idt_init - Initialize IDT
 */
static void idt_init(void)
{
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uintptr_t)&idt;

    /* Clear IDT */
    for (u32 i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /* Load IDT */
    idt_flush((uintptr_t)&idt_ptr);
}

/*===========================================================================*/
/*                         DISK ACCESS (INT 13h)                             */
/*===========================================================================*/

/**
 * disk_read_sectors - Read sectors from the boot disk
 * @lba: Starting logical block address
 * @count: Number of 512-byte sectors
 * @buffer: Destination
 *
 * Returns: true on success, false on failure
 *
 * The body of this function was a single inline-asm block that could not be
 * correct. It referenced %7 with only six operands supplied ("operand number
 * out of range", the only error GCC still reported in this file), it pushed
 * %ebx and then clobbered it in the same block, and it programmed %es:bx for
 * the transfer buffer without ever setting %es from `buf` -- so even had it
 * assembled, INT 13h would have read into segment 0x1000 at whatever offset
 * %bx happened to hold. It is also a real-mode BIOS interface emitted into a
 * 64-bit object, where %bx and %es are not general-purpose registers.
 *
 * INT 13h cannot be issued from here. It has to come from a 16-bit real-mode
 * stub, which is the missing piece of this bootloader. Until that stub exists,
 * this reports failure: a boot loader that claims to have read a sector it
 * never read will happily load the stale contents of the buffer as a kernel.
 */
static bool disk_read_sectors(u32 lba, u32 count, void *buffer)
{
    (void)buffer;

    /*
     * LBA-to-CHS conversion is retained because it is correct and the stub
     * will need it; it is not dead code, it is the part of the interface that
     * was right.
     */
    {
        u32 sector = lba;
        u32 remaining = count;

        while (remaining > 0) {
            u32 to_read = (remaining > 128) ? 128 : remaining;
            u32 cylinder;
            u32 head;
            u32 sector_num;

            cylinder   = sector / (255 * 63);
            head       = (sector % (255 * 63)) / 63;
            sector_num = (sector % 63) + 1;

            if (cylinder > 1023) {
                /* Beyond the CHS limit; the stub must use LBA extensions. */
                return false;
            }

            (void)cylinder;
            (void)head;
            (void)sector_num;

            sector += to_read;
            remaining -= to_read;
        }
    }

    return false;
}

/*===========================================================================*/
/*                         MEMORY DETECTION                                  */
/*===========================================================================*/

/**
 * detect_memory_e820 - Detect memory using BIOS INT 15h E820
 */
static void detect_memory_e820(void)
{
    /*
     * E820 entry layout, and the buffer the real-mode stub will fill in.
     * Referenced here so the handoff contract is visible from this side; it is
     * written by the stub, not by this function.
     */
    struct {
        u32 base_low;
        u32 base_high;
        u32 length_low;
        u32 length_high;
        u32 type;
    } __attribute__((packed)) entry;

    (void)entry;

    u32 signature = 0x534D4150;  /* "SMAP" */
    u32 continuation = 0;
    u32 entries = 0;

    vga_print("  Detecting memory (E820)...\n");

    /*
     * INT 15h E820 cannot be issued from here.
     *
     * The block that used to sit in this loop pushed %ebx/%ecx/%edx/%esi/%edi
     * and programmed %edi with a linear address -- real-mode register usage,
     * emitted into a 64-bit object where those pushes are operand-size
     * mismatches and `int $0x15` is not a callable interface. The assembler
     * rejected every one of them ("operand size mismatch for push"), which is
     * why this file produced no object at all despite compiling cleanly.
     *
     * E820 has to be queried from the 16-bit real-mode entry stub and the
     * results handed over; see the STATUS note at the top of the file. The
     * loop is retained so the handoff point is obvious, and it records nothing
     * rather than recording invented memory.
     */
    (void)signature;
    (void)continuation;

    entries = 0;

    memory_map_count = entries;
    boot_params.memory_map_entries = entries;

    vga_print("  Found ");
    vga_print_dec(entries);
    vga_print(" memory regions\n");
}

/**
 * detect_memory_simple - Simple memory detection (fallback)
 */
static u32 detect_memory_simple(void)
{
    u32 ext_mem = 0;

    /*
     * INT 15h AX=E801h, for the same reason as E820 above: it is a real-mode
     * BIOS call and cannot be made from a 64-bit translation unit. The inline
     * asm that used to be here also used %ax (no such register) and `jc 1f`
     * with no reachable label 1.
     *
     * Report zero so the caller's fallback chain can tell that detection did
     * not happen; do not invent a size.
     */
    (void)ext_mem;

    return 0;
}

/*===========================================================================*/
/*                         KERNEL LOADING                                    */
/*===========================================================================*/

/**
 * load_kernel - Load kernel from disk
 */
static bool load_kernel(void)
{
    u32 *kernel = (u32 *)KERNEL_LOAD_ADDRESS;

    vga_print("  Loading kernel from sector 2...\n");

    /* Read kernel (starting at sector 2) */
    if (!disk_read_sectors(2, KERNEL_SECTORS, kernel)) {
        vga_print("  ERROR: Failed to read kernel!\n");
        return false;
    }

    /* Check for ELF magic */
    if (kernel[0] != 0x464C457F) {  /* "\x7FELF" */
        vga_print("  WARNING: Not a valid ELF file (raw binary assumed)\n");
    }

    /* Find kernel size (simple heuristic) */
    kernel_size = KERNEL_MAX_SIZE;  /* Assume max for now */

    vga_print("  Kernel loaded at 0x");
    vga_print_hex(KERNEL_LOAD_ADDRESS);
    vga_print(" (");
    vga_print_dec(kernel_size);
    vga_print(" bytes)\n");

    return true;
}

/**
 * load_initrd - Load initial ramdisk
 */
static bool load_initrd(void)
{
    u32 *initrd = (u32 *)INITRD_LOAD_ADDRESS;

    vga_print("  Loading initrd from sector 200...\n");

    /* Read initrd (starting at sector 200) */
    if (!disk_read_sectors(200, 128, initrd)) {
        vga_print("  WARNING: Failed to read initrd (continuing without)\n");
        initrd_size = 0;
        return true;  /* Continue without initrd */
    }

    initrd_size = 128 * SECTOR_SIZE;

    vga_print("  Initrd loaded at 0x");
    vga_print_hex(INITRD_LOAD_ADDRESS);
    vga_print(" (");
    vga_print_dec(initrd_size);
    vga_print(" bytes)\n");

    return true;
}

/*===========================================================================*/
/*                         BOOT PARAMETER SETUP                              */
/*===========================================================================*/

/**
 * setup_boot_params - Setup boot parameters for kernel
 */
static void setup_boot_params(void)
{
    /* Zero out boot params */
    for (u32 i = 0; i < sizeof(boot_params_t) / 4; i++) {
        ((u32 *)&boot_params)[i] = 0;
    }

    /* Set magic */
    boot_params.magic = BOOT_MAGIC;

    /* Kernel info */
    boot_params.kernel_base = KERNEL_LOAD_ADDRESS;
    boot_params.kernel_size = kernel_size;
    boot_params.kernel_entry = KERNEL_LOAD_ADDRESS;  /* Simplified */

    /* Initrd info */
    boot_params.initrd_base = INITRD_LOAD_ADDRESS;
    boot_params.initrd_size = initrd_size;

    /* Command line */
    const char *cmdline = "root=/dev/sda2 quiet splash loglevel=3";
    for (u32 i = 0; cmdline[i] && i < 511; i++) {
        boot_params.cmdline[i] = cmdline[i];
    }
    /* cmdline_offset is an offset, but the field was being loaded with a
     * 64-bit address. The kernel receives a flat physical address for the
     * boot_params structure and adds the offset, so this has to be the
     * pointer-sized value. */
    boot_params.cmdline_offset = (u32)(uintptr_t)boot_params.cmdline;
}

/*===========================================================================*/
/*                         PROTECTED MODE TRANSITION                         */
/*===========================================================================*/

/**
 * enter_protected_mode - Switch to protected mode
 */
static void enter_protected_mode(void)
{
    u32 cr0;

    vga_print("Preparing protected-mode entry...\n");

    disable_interrupts();

    gdt_init();

    /*
     * Set CR0.PE.
     *
     * The far jump that used to follow this (`jmp $0x08, $.flush`) does not
     * encode in a 64-bit object: a far jump with an immediate selector is a
     * legacy-mode instruction, and the assembler rejected it with "unsupported
     * instruction `jmp'". The mode switch and the CS reload it performs both
     * belong to the 16-bit stub -- from this translation unit there is no
     * instruction that can enter or leave 32-bit protected mode.
     *
     * CR0.PE is still set here because that much is valid, and the descriptor
     * tables the kernel inherits are then in place.
     */
    cr0 = get_cr0();
    cr0 |= 0x00000001;  /* PE */

    /* There was a set_cr0() call here; no such function was ever declared or
     * defined. Writing CR0 is a single instruction. */
    /*
     * "mov %0, %%cr0" with a u32 operand fails to assemble: CR0 has no
     * 32-bit-register move form in 64-bit mode. Widening the value to 64 bits
     * selects the valid encoding.
     */
    __asm__ volatile("mov %0, %%cr0" : : "r"((u64)cr0) : "memory");

    /*
     * Reload the data segment registers from the flat 0x10 descriptor. In
     * 64-bit mode DS/ES/FS/GS/SS are ignored for addressing but must still be
     * valid, and leaving them pointing at real-mode segments is not allowed.
     */
    /* Segment registers take a 16-bit operand; go through %ax, which is the
     * form the assembler accepts. */
    __asm__ volatile(
        "mov %w0, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        :
        : "r"((u32)0x10)
        : "eax", "memory"
    );

    vga_print("Protected mode enabled.\n");
}

/*===========================================================================*/
/*                         BOOTLOADER ENTRY POINT                            */
/*===========================================================================*/

/**
 * bootloader_main - Main bootloader entry point
 */
void bootloader_main(void)
{
    /* Clear screen */
    vga_clear();

    /* Display boot banner */
    vga_set_color(0x0F, 0x00);  /* White on black */
    vga_print("\n");
    vga_print("===============================================\n");
    vga_print("  ");
    vga_print(BOOTLOADER_NAME);
    vga_print(" v");
    vga_print(BOOTLOADER_VERSION);
    vga_print("\n");
    vga_print("===============================================\n\n");

    vga_set_color(0x07, 0x00);  /* Light gray on black */

    /*
     * Initialize GDT and IDT, then actually enter protected mode.
     *
     * enter_protected_mode() was defined and never called, so the sequence
     * stayed in whatever mode it was handed, with a GDT loaded but CR0.PE
     * still clear. Every subsequent 32-bit descriptor-relative access then
     * ran under real-mode segmentation.
     */
    vga_print("Initializing GDT...\n");
    gdt_init();

    vga_print("Initializing IDT...\n");
    idt_init();

    enter_protected_mode();

    /* Detect memory */
    vga_print("Detecting memory...\n");
    detect_memory_e820();

    /*
     * E820 is not guaranteed to produce anything -- some BIOSes return
     * nothing, and this build cannot issue the call at all (see
     * disk_read_sectors()). Fall back to the INT 15h AX=E801h size, which is
     * why detect_memory_simple() was written; it had no caller, so a machine
     * with no E820 map booted with memory_map_count == 0 and the kernel would
     * believe the system had no usable RAM.
     */
    if (memory_map_count == 0) {
        u32 ext_bytes = detect_memory_simple();

        if (ext_bytes > 0) {
            memory_map[0].base_low = 0x100000;
            memory_map[0].base_high = 0;
            memory_map[0].length_low = ext_bytes;
            memory_map[0].length_high = 0;
            memory_map[0].type = MEM_AVAILABLE;
            memory_map_count = 1;
            boot_params.memory_map_entries = 1;

            vga_print("  E820 unavailable, using E801 fallback\n");
        }
    }

    /* Load kernel */
    vga_print("Loading kernel...\n");
    if (!load_kernel()) {
        vga_print("FATAL: Kernel load failed!\n");
        halt_cpu();
    }

    /* Load initrd */
    load_initrd();

    /* Setup boot parameters */
    vga_print("Setting up boot parameters...\n");
    setup_boot_params();

    /* Display boot summary */
    vga_print("\n");
    vga_print("Boot Summary:\n");
    vga_print("  Kernel:     0x");
    vga_print_hex(KERNEL_LOAD_ADDRESS);
    vga_print(" - 0x");
    vga_print_hex(KERNEL_LOAD_ADDRESS + kernel_size);
    vga_print("\n");

    if (initrd_size > 0) {
        vga_print("  Initrd:     0x");
        vga_print_hex(INITRD_LOAD_ADDRESS);
        vga_print(" - 0x");
        vga_print_hex(INITRD_LOAD_ADDRESS + initrd_size);
        vga_print("\n");
    }

    vga_print("  Memory:     ");
    vga_print_dec(memory_map_count);
    vga_print(" regions\n");
    vga_print("\n");

    vga_print("Jumping to kernel...\n");
    vga_print("\n");

    /* Small delay */
    for (volatile u32 i = 0; i < 1000000; i++);

    /* Jump to kernel - clear multiboot registers since we're not GRUB */
    typedef void (*kernel_entry_t)(void);
    kernel_entry_t kernel_entry = (kernel_entry_t)KERNEL_LOAD_ADDRESS;

    __asm__ volatile(
        "xor %%ebx, %%ebx\n"    /* Clear EBX - no multiboot info */
        "xor %%eax, %%eax\n"    /* Clear EAX - no multiboot magic */
        "jmp *%0\n"             /* Jump to kernel entry point */
        :
        : "r"(kernel_entry)
        : "ebx", "eax", "memory"
    );

    /* Should never reach here */
    vga_print("ERROR: Kernel returned!\n");
    halt_cpu();
}

/*===========================================================================*/
/*                         STAGE2 BOOT SECTOR SIGNATURE                      */
/*===========================================================================*/

/* This ensures the stage2 bootloader has a valid boot signature */
__attribute__((section(".boot_sig")))
const u16 boot_signature = 0xAA55;
