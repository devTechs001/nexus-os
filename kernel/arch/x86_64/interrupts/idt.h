/*
 * NEXUS OS - Interrupt Descriptor Table
 * kernel/arch/x86_64/interrupts/idt.h
 */

#ifndef _NEXUS_ARCH_X86_64_IDT_H
#define _NEXUS_ARCH_X86_64_IDT_H

#include "../../../include/types.h"

/* IDT Entry Structure (x86_64) */
struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8  ist;            /* Interrupt Stack Table */
    u8  type_attr;      /* Type and attributes */
    u16 offset_mid;
    u32 offset_high;
    u32 reserved;
} __attribute__((packed));

/* IDT Descriptor (IDTR) */
struct idt_ptr {
    u16 limit;
    u64 base;
} __attribute__((packed));

/* IDT Constants */
#define IDT_ENTRIES         256
#define IDT_GATE_INTERRUPT  0x8E
#define IDT_GATE_TRAP       0x8F
#define IDT_GATE_USER       0x60

void arch_init_idt(void);

#endif /* _NEXUS_ARCH_X86_64_IDT_H */
