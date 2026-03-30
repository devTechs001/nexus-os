/*
 * NEXUS OS - Interrupt Descriptor Table Implementation
 * kernel/arch/x86_64/interrupts/idt.c
 */

#include "idt.h"
#include "../../../include/kernel.h"

/* ISR Stub Table (from assembly) */
extern u64 isr_stub_table[];

/* IDT Entries */
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtr;

/**
 * set_idt_gate - Set an IDT gate
 */
static void set_idt_gate(int num, u64 base, u16 sel, u8 flags)
{
    idt[num].offset_low = (base & 0xFFFF);
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_mid = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].reserved = 0;
}

/**
 * arch_init_idt - Initialize the Interrupt Descriptor Table
 */
void arch_init_idt(void)
{
    /* Load ISR stubs from the table */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        set_idt_gate(i, isr_stub_table[i], 0x08, IDT_GATE_INTERRUPT);
    }

    /* Set up IDTR */
    idtr.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idtr.base = (u64)&idt;

    /* Load IDT */
    __asm__ __volatile__("lidt %0" : : "m"(idtr));

    pr_info("x86_64 IDT initialized (%d gates)\n", IDT_ENTRIES);
}

/**
 * Registers structure (matches the order pushed in assembly)
 */
struct registers {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rdi, rsi, rbp, rbx, rdx, rcx, rax;
    u64 isr_num, error_code;
    u64 rip, cs, rflags, rsp, ss;
};

/**
 * interrupt_handler - Main interrupt handler (called from assembly)
 */
void interrupt_handler(struct registers *regs)
{
    /* Exception handling */
    if (regs->isr_num < 32) {
        pr_err("EXCEPTION %d (error 0x%X) at 0x%llX\n", 
               (u32)regs->isr_num, (u32)regs->error_code, regs->rip);
        
        kernel_panic("CPU Exception: %d", (u32)regs->isr_num);
    }

    /* IRQ handling would go here */
}
