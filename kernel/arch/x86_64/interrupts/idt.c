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

        /*
         * A single line naming the vector and RIP is not enough to act on.
         * Several exception kinds look alike from the outside:
         *
         *  - #PF carries the faulting address in CR2 and a meaningful error
         *    code. A CR2 of 0 with error 0 means CR2 is stale.
         *  - Accessing a non-canonical address raises #GP with error code 0
         *    and leaves CR2 untouched - the faulting pointer is in a
         *    register, not in CR2.
         *  - A bad indirect call or a corrupt return address shows up as #GP
         *    with the destination in RIP and the culprit on the stack.
         *
         * Distinguishing these needs the register file, the stack pointer and
         * the bytes the CPU was executing. Dump all three.
         */
        pr_err("  RIP=0x%016llX  CS=0x%04llX  RFLAGS=0x%016llX\n",
               regs->rip, regs->cs, regs->rflags);
        pr_err("  RSP=0x%016llX  SS=0x%04llX\n", regs->rsp, regs->ss);
        pr_err("  RAX=0x%016llX  RBX=0x%016llX  RCX=0x%016llX\n",
               regs->rax, regs->rbx, regs->rcx);
        pr_err("  RDX=0x%016llX  RSI=0x%016llX  RDI=0x%016llX\n",
               regs->rdx, regs->rsi, regs->rdi);
        pr_err("  RBP=0x%016llX  R8 =0x%016llX  R9 =0x%016llX\n",
               regs->rbp, regs->r8, regs->r9);
        pr_err("  R10=0x%016llX  R11=0x%016llX  R12=0x%016llX\n",
               regs->r10, regs->r11, regs->r12);
        pr_err("  R13=0x%016llX  R14=0x%016llX  R15=0x%016llX\n",
               regs->r13, regs->r14, regs->r15);

        /*
         * The instruction bytes at RIP. If RIP points into a function that
         * could not plausibly be executing, this confirms a corrupt return
         * address or a bad indirect call rather than a genuine fault inside
         * that function.
         */
        {
            /*
             * Only read the bytes if RIP is a canonical, kernel-space address
             * that the current page table actually maps. Reading an unmapped or
             * non-canonical address here would fault again inside the fault
             * handler and turn a diagnosable crash into a triple fault with no
             * output at all.
             */
            u64 rip = regs->rip;
            u64 canonical = (u64)((s64)((rip << 16) >> 16));

            if (rip == canonical && rip >= 0x100000ULL) {  /* kernel image starts at 1 MB */
                const u8 *code = (const u8 *)(uintptr_t)rip;

                pr_err("  code:");
                for (int i = 0; i < 8; i++) {
                    pr_err(" %02X", code[i]);
                }
                pr_err("\n");
            } else {
                pr_err("  RIP is not a mapped kernel address; not dumping code\n");
            }
        }

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
