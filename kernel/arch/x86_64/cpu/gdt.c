/*
 * NEXUS OS - Global Descriptor Table Implementation
 * kernel/arch/x86_64/cpu/gdt.c
 */

#include "gdt.h"
#include "../../../include/kernel.h"

/* GDT Entries */
static struct gdt_entry gdt[8];
static struct gdt_ptr gdtr;
static struct tss_entry tss;

/**
 * set_gdt_entry - Set a GDT entry
 */
static void set_gdt_entry(int num, u32 base, u32 limit, u8 access, u8 gran)
{
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_mid = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F);

    gdt[num].granularity |= (gran & 0xF0);
    gdt[num].access = access;
}

/**
 * set_gdt_entry_64 - Set a 64-bit GDT entry (TSS)
 */
static void set_gdt_entry_64(int num, u64 base, u32 limit, u8 access, u8 gran)
{
    struct gdt_entry_64 *entry = (struct gdt_entry_64 *)&gdt[num];

    entry->limit_low = (limit & 0xFFFF);
    entry->base_low = (base & 0xFFFF);
    entry->base_mid = (base >> 16) & 0xFF;
    entry->base_high = (base >> 24) & 0xFF;
    entry->base_upper = (base >> 32) & 0xFFFFFFFF;
    entry->reserved = 0;

    entry->access = access;
    entry->granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
}

/**
 * arch_init_gdt - Initialize the Global Descriptor Table
 */
void arch_init_gdt(void)
{
    /* Null descriptor */
    set_gdt_entry(0, 0, 0, 0, 0);

    /* Kernel Code (Ring 0, 64-bit) */
    set_gdt_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xAF);

    /* Kernel Data (Ring 0, 64-bit) */
    set_gdt_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* User Code (Ring 3, 32-bit - for compatibility) */
    set_gdt_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* User Data (Ring 3, 64-bit) */
    set_gdt_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    /* User Code (Ring 3, 64-bit) */
    set_gdt_entry(5, 0, 0xFFFFFFFF, 0xFA, 0xAF);

    /* TSS (Task State Segment) - uses 2 GDT entries */
    memset(&tss, 0, sizeof(struct tss_entry));
    tss.iopb_offset = sizeof(struct tss_entry);
    
    set_gdt_entry_64(6, (u64)&tss, sizeof(struct tss_entry) - 1, 0x89, 0x00);

    /* Set up GDTR */
    gdtr.limit = (sizeof(struct gdt_entry) * 8) - 1;
    gdtr.base = (u64)&gdt;

    /* Load GDT */
    __asm__ __volatile__("lgdt %0" : : "m"(gdtr));

    /* Reload segment registers */
    __asm__ __volatile__(
        "push $0x08\n"          /* Kernel Code Selector */
        "lea 1f(%%rip), %%rax\n"
        "push %%rax\n"
        "lretq\n"               /* Long return to reload CS */
        "1:\n"
        "mov $0x10, %%ax\n"     /* Kernel Data Selector */
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : : "rax", "ax", "memory"
    );

    /* Load TSS */
    __asm__ __volatile__("ltr %%ax" : : "a"(GDT_TSS));

    pr_info("x86_64 GDT and TSS initialized\n");
}
