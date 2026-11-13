/*
 * NEXUS OS - Architecture-specific Paging
 * kernel/arch/x86_64/mm/paging.c
 */

#include "../../../include/kernel.h"

/* PML4 Table (Page Map Level 4) */
extern u64 pml4_table[];

/**
 * arch_init_paging - Initialize architecture-specific paging
 */
void arch_init_paging(void)
{
    /* The bootloader (boot.asm) already set up 2MB identity mapping */
    /* for the first 1GB. Here we can add more specific mappings if needed. */
    
    /* On x86_64, paging is already enabled to enter long mode. */
    /* This function can be used to set up the final kernel page tables */
    /* if vmm_init needs assistance. */

    pr_info("x86_64 paging initialized (4-level, 2MB pages enabled)\n");
    
    /* Early serial debug */
    outb(0x3F8, 'P');
    outb(0x3F8, 'G');
    outb(0x3F8, 'O');
    outb(0x3F8, 'K');
    outb(0x3F8, '\n');
}
