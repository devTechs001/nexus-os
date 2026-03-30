/*
 * NEXUS OS - Global Descriptor Table
 * kernel/arch/x86_64/cpu/gdt.h
 */

#ifndef _NEXUS_ARCH_X86_64_GDT_H
#define _NEXUS_ARCH_X86_64_GDT_H

#include "../../../include/types.h"

/* GDT Selectors */
#define GDT_KERNEL_CODE     0x08
#define GDT_KERNEL_DATA     0x10
#define GDT_USER_CODE_32    0x18
#define GDT_USER_DATA       0x20
#define GDT_USER_CODE_64    0x28
#define GDT_TSS             0x30

/* GDT Entry Structure */
struct gdt_entry {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  granularity;
    u8  base_high;
} __attribute__((packed));

/* GDT Descriptor (GDTR) */
struct gdt_ptr {
    u16 limit;
    u64 base;
} __attribute__((packed));

/* TSS Structure (x86_64) */
struct tss_entry {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iopb_offset;
} __attribute__((packed));

/* GDT Entry (16-byte version for TSS) */
struct gdt_entry_64 {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;
    u8  access;
    u8  granularity;
    u8  base_high;
    u32 base_upper;
    u32 reserved;
} __attribute__((packed));

void arch_init_gdt(void);

#endif /* _NEXUS_ARCH_X86_64_GDT_H */
