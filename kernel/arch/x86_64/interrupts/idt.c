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
 *
 * The assembly stub pushes, in this order:
 *   rax rcx rdx rbx rbp rsi rdi r8 r9 r10 r11 r12 r13 r14 r15
 * and therefore memory at RSP (lowest address) holds r15 first.
 */
struct registers {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rdi, rsi, rbp, rbx, rdx, rcx, rax;
    u64 isr_num, error_code;
    u64 rip, cs, rflags, rsp, ss;
};

/* Legacy PIC end-of-interrupt. Declared locally so idt.c does not need to
 * pull in the whole HAL header set. */
extern void pic_send_eoi(unsigned char irq);

/* PIT tick counter and scheduler tick, provided elsewhere. */
#include "../cpu/timer.h"
extern void scheduler_tick(void);

/**
 * interrupt_handler - Main interrupt handler (called from assembly)
 */
void interrupt_handler(struct registers *regs)
{
    /* Exception handling */
    if (regs->isr_num < 32) {
        u64 cr2;
        __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
        pr_err("EXCEPTION %d (error 0x%X) at 0x%llX, CR2=0x%llX\n", 
               (u32)regs->isr_num, (u32)regs->error_code, regs->rip, cr2);
        
        kernel_panic("CPU Exception: %d", (u32)regs->isr_num);
    }

    /*
     * Hardware IRQs (the legacy PIC remaps IRQ0-15 onto vectors 32-47).
     *
     * An end-of-interrupt MUST be sent, otherwise the PIC keeps the line
     * asserted and immediately re-delivers the same IRQ. That turns the
     * first timer tick into an endless interrupt storm which starves the
     * kernel and looks like a hang/panic in the late boot stage.
     */
    if (regs->isr_num >= 32 && regs->isr_num < 48) {
        u32 irq = (u32)regs->isr_num - 32;

        /*
         * IRQ0 is the PIT tick. Service it, otherwise the kernel clock
         * never advances, delay_ms() spins forever and the scheduler
         * never sees a tick.
         *
         * Note: deliberately no per-IRQ logging here. printk() writes to
         * the serial port with a busy-wait on the UART, so logging every
         * interrupt would stall the system and flood the console.
         */
        if (irq == 0) {
            timer_tick();
            scheduler_tick();
        }

        pic_send_eoi((unsigned char)irq);
    }
}
