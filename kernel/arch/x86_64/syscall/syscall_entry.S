/*
 * NEXUS OS - System Call Entry/Exit (SYSCALL / SYSRET)
 * kernel/arch/x86_64/syscall/syscall_entry.S
 *
 * This file was missing entirely. kernel/syscall/syscall.c declared
 * syscall_entry_asm() and syscall_exit_asm() as extern and nothing defined
 * them, so the ring-3 half of the system call path did not exist: no
 * instruction a user program could execute to enter the kernel, no MSR setup
 * to make SYSCALL work, and no way back out.
 *
 * SYSCALL vectors to IA32_LSTAR with, per the architecture:
 *
 *   RCX = return RIP        (set by the CPU)
 *   R11 = return RFLAGS     (set by the CPU)
 *   RSP = UNCHANGED         (still the user stack - we must swap it)
 *   RAX = syscall number, RDI/RSI/RDX/R10/R8/R9 = arguments
 *
 * The argument registers match the SysV AMD64 convention for the first five,
 * with rcx replaced by r10 for the fourth, which is why the kernel side can
 * dispatch with an ordinary C call.
 *
 * Written for GNU as rather than NASM so it is assembled by the same toolchain
 * as the rest of the kernel and can be verified in place.
 */

#define MSR_EFER      0xC0000080
#define MSR_STAR      0xC0000081
#define MSR_LSTAR     0xC0000082
#define MSR_SFMASK    0xC0000084

#define EFER_SCE      (1 << 0)
#define KERNEL_CS     0x08
#define SYSRET_BASE   0x18

/*
 * Offsets into struct syscall_frame (kernel/syscall/syscall.h). Kept as
 * constants so the exit path can index the frame instead of popping it, which
 * would destroy the return value in rax.
 */
#define FR_R15          0
#define FR_R14          8
#define FR_R13          16
#define FR_R12          24
#define FR_R11          32
#define FR_R10          40
#define FR_R9           48
#define FR_R8           56
#define FR_RDI          64
#define FR_RSI          72
#define FR_RDX          80
#define FR_RCX          88
#define FR_RAX          96
#define FR_ERROR_CODE   104
#define FR_INTERRUPT_NR 112
#define FR_RIP          120
#define FR_CS           128
#define FR_RFLAGS       136
#define FR_RSP          144
#define FR_SS           152

    .section .bss
    .align 16
/*
 * Scratch slots used to carry user context across the stack switch. A single
 * static set is correct for a uniprocessor kernel; a per-CPU area would be
 * needed before this could run on SMP.
 */
user_rsp_save:      .quad 0
user_rip_save:      .quad 0
user_rflags_save:   .quad 0

    .align 16
    .globl kernel_syscall_stack_top
kernel_syscall_stack_top: .quad 0

    .text
    .globl syscall_entry_asm
    .type  syscall_entry_asm, @function
/*
 * syscall_entry_asm - Entered by SYSCALL from ring 3
 */
syscall_entry_asm:
    /*
     * RSP still points at the user stack. It is not trustworthy, and cannot be
     * pushed onto, so park the user context in memory before touching any
     * stack at all. RCX and R11 hold the return RIP and RFLAGS the CPU set.
     */
    movq %rsp, user_rsp_save(%rip)
    movq %rcx, user_rip_save(%rip)
    movq %r11, user_rflags_save(%rip)

    /* Move to the kernel stack. */
    movq kernel_syscall_stack_top(%rip), %rsp
    testq %rsp, %rsp
    jz    .Lno_stack

    /*
     * Build a struct syscall_frame so syscall_dispatch() sees the same layout
     * it sees on the interrupt path. Pushes run from the highest offset down,
     * so they are emitted in reverse struct order; twenty 8-byte pushes total.
     */
    pushq $0                              /* ss              (FR_SS)      */
    pushq user_rsp_save(%rip)             /* rsp             (FR_RSP)     */
    pushq user_rflags_save(%rip)          /* rflags          (FR_RFLAGS)  */
    pushq $0                              /* cs              (FR_CS)      */
    pushq user_rip_save(%rip)             /* rip             (FR_RIP)     */
    pushq $0                              /* interrupt_nr                 */
    pushq $0                              /* error_code                   */
    pushq %rax                            /* rax  = syscall number        */
    pushq user_rip_save(%rip)             /* rcx  = return rip            */
    pushq %rdx                            /* rdx  = arg3                  */
    pushq %rsi                            /* rsi  = arg2                  */
    pushq %rdi                            /* rdi  = arg1                  */
    pushq %r8                             /* r8   = arg5                  */
    pushq %r9                             /* r9   = arg6                  */
    pushq %r10                            /* r10  = arg4                  */
    pushq user_rflags_save(%rip)          /* r11  = return rflags         */
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* rdi = pointer to the frame; SysV wants rsp 16-byte aligned at the call.
     * Twenty pushes from an aligned base leave it aligned. */
    movq %rsp, %rdi
    call syscall_dispatch

    /*
     * rax holds the result. Restore the user's callee-saved registers from the
     * frame, then return with sysretq.
     *
     * Do NOT pop the frame: the first pop targets r15 and would overwrite the
     * result in rax before it could be returned.
     */
    movq FR_R15(%rsp), %r15
    movq FR_R14(%rsp), %r14
    movq FR_R13(%rsp), %r13
    movq FR_R12(%rsp), %r12

    /* SYSRET takes the target RIP in rcx and RFLAGS in r11. */
    movq FR_RIP(%rsp), %rcx
    movq FR_RFLAGS(%rsp), %r11

    /*
     * Switch back to the user stack last: once rsp moves, the frame above is
     * unreachable.
     */
    movq user_rsp_save(%rip), %rsp

    sysretq

.Lno_stack:
    /*
     * No kernel stack was installed, so there is nowhere to build a frame.
     * Halt with interrupts off rather than faulting recursively on a user
     * stack.
     */
    cli
.Lhang_nostack:
    hlt
    jmp .Lhang_nostack
    .size syscall_entry_asm, . - syscall_entry_asm

    .globl syscall_exit_asm
    .type  syscall_exit_asm, @function
/*
 * syscall_exit_asm - exists only because syscall.c declares it
 *
 * syscall_entry_asm performs the return itself with sysretq. Reaching this
 * symbol directly means the caller expected a separate exit step that does not
 * exist, so stop loudly rather than returning into nothing.
 */
syscall_exit_asm:
    cli
.Lhang_exit:
    hlt
    jmp .Lhang_exit
    .size syscall_exit_asm, . - syscall_exit_asm

    .globl syscall_init_msrs
    .type  syscall_init_msrs, @function
/*
 * syscall_init_msrs - Program EFER.SCE, STAR, LSTAR and SFMASK
 *
 * Called from syscall_init(). None of these were ever written, so SYSCALL was
 * not even enabled and executing it faulted.
 *
 *   EFER.SCE    enables the SYSCALL/SYSRET instructions
 *   STAR[47:32] kernel CS for SYSCALL entry
 *   STAR[63:48] base for SYSRET: user CS = base+16, user SS = base+8
 *   LSTAR       entry RIP
 *   SFMASK      RFLAGS bits cleared on entry
 */
syscall_init_msrs:
    pushq %rbx

    /* EFER.SCE */
    movl $MSR_EFER, %ecx
    rdmsr
    orl  $EFER_SCE, %eax
    wrmsr

    /*
     * STAR. With base 0x18, SYSRET loads CS = 0x28 (GDT_USER_CODE_64, a 64-bit
     * ring-3 code segment) and SS = 0x20 (GDT_USER_DATA). That is exactly the
     * layout arch_init_gdt() already installs, so the GDT needs no change -
     * the selectors were there all along and simply never used.
     */
    movl $MSR_STAR, %ecx
    movl $(KERNEL_CS << 16), %eax
    movl $(SYSRET_BASE << 16), %edx
    wrmsr

    /* LSTAR = syscall_entry_asm */
    movl $MSR_LSTAR, %ecx
    movq $syscall_entry_asm, %rax
    movq %rax, %rdx
    shrq $32, %rdx
    wrmsr

    /*
     * SFMASK: clear IF so the entry path runs with interrupts off, and TF so a
     * stray trap flag cannot single-step the kernel.
     */
    movl $MSR_SFMASK, %ecx
    movl $0x300, %eax
    xorl %edx, %edx
    wrmsr

    popq %rbx
    ret
    .size syscall_init_msrs, . - syscall_init_msrs

    .globl syscall_set_kernel_stack
    .type  syscall_set_kernel_stack, @function
/*
 * syscall_set_kernel_stack - install the kernel stack used on SYSCALL entry
 */
syscall_set_kernel_stack:
    movq %rdi, kernel_syscall_stack_top(%rip)
    ret
    .size syscall_set_kernel_stack, . - syscall_set_kernel_stack

    .globl syscall_get_kernel_stack
    .type  syscall_get_kernel_stack, @function
/*
 * syscall_get_kernel_stack - read back the installed stack (diagnostics)
 */
syscall_get_kernel_stack:
    movq kernel_syscall_stack_top(%rip), %rax
    ret
    .size syscall_get_kernel_stack, . - syscall_get_kernel_stack

/*
 *===========================================================================
 * Ring 3 transition
 *===========================================================================
 *
 * The kernel had no path from ring 0 into ring 3. There was no iretq that
 * lowered privilege, no way to set up a user stack, and arch_switch_to() did
 * not exist, so a loaded ELF image could never be executed even though the
 * loader placed it correctly in memory.
 *
 * enter_user_mode(entry, user_stack_top, arg) drops to ring 3 and calls
 * entry(arg). It never returns to the caller: the only way back into the
 * kernel is a system call or an interrupt.
 */
    .text
    .globl enter_user_mode
    .type  enter_user_mode, @function
enter_user_mode:
    /*
     * rdi = user entry point
     * rsi = user stack top
     * rdx = argument passed in rdi to the entry point
     *
     * Build the iretq frame in the order iretq expects:
     *   SS, RSP, RFLAGS, CS, RIP
     * then push the argument so it is popped into rdi afterwards.
     */
    cli

    /* SS = user data selector (ring 3) */
    pushq $0x20
    /* RSP = user stack top */
    pushq %rsi
    /* RFLAGS: IF set (0x200) and bit 1 reserved-set */
    pushq $0x202
    /* CS = user code selector (ring 3) */
    pushq $0x28
    /* RIP = user entry point */
    pushq %rdi

    /* Argument for the entry point, consumed by the pop below. */
    pushq %rdx

    /*
     * Clear every data segment to the user selector and zero the general
     * purpose registers, so no kernel value leaks into the new address space.
     */
    movq $0x20, %rax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    xorq %rax, %rax
    xorq %rbx, %rbx
    xorq %rcx, %rcx
    xorq %rdx, %rdx
    xorq %rsi, %rsi
    xorq %rbp, %rbp
    xorq %r8, %r8
    xorq %r9, %r9
    xorq %r10, %r10
    xorq %r11, %r11
    xorq %r12, %r12
    xorq %r13, %r13
    xorq %r14, %r14
    xorq %r15, %r15

    /* rdi = the entry argument pushed above, then drop the slot. */
    popq %rdi

    iretq
    .size enter_user_mode, . - enter_user_mode

    .section .note.GNU-stack,"",@progbits
