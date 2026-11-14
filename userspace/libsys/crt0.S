/*
 * NEXUS OS - userspace entry point
 * userspace/libsys/crt0.S
 *
 * The kernel enters a user program by building an iret/sysret frame that lands
 * at the image's ELF entry point with rsp pointing at the top of a freshly
 * allocated user stack, and rdi holding a single argument (see
 * enter_user_mode() in kernel/arch/x86_64/syscall/syscall_entry.S).
 *
 * Nothing else is set up. There is no libc startup routine, no dynamic linker,
 * and no atexit chain, so this stub has to do the three things a process needs
 * before any C code is safe to run:
 *
 *   1. Establish a frame pointer and a 16-byte aligned stack. The System V ABI
 *      requires rsp % 16 == 0 at the call instruction, so that a function
 *      prologue's push %rbp leaves rsp % 16 == 8 on entry, as compiled code
 *      expects.
 *   2. Call main(argc, argv, envp).
 *   3. Hand the return value to SYS_EXIT. Falling through into whatever
 *      follows main() in .text would execute the next function with a garbage
 *      stack frame.
 *
 * The kernel passes no command line and no environment, so argc/argv/envp are
 * synthesised here: argv[0] is an empty string and everything else is NULL.
 * Frameworks in this tree print argv[0] and iterate the arrays, so they must
 * be real, dereferenceable pointers and not NULL.
 */

    .section .text.entry, "ax"
    .code64
    .globl _start
    .type _start, @function

_start:
    /*
     * rax = the value the kernel put in rdi (the entry argument).
     *
     * Preserved on the stack because everything below clobbers the argument
     * registers. It is conventionally the program's initial "stack pointer" or
     * a boot flag; nothing in this tree interprets it yet.
     */
    .globl _start_boot_arg
_start_boot_arg:
    pushq %rdi

    /* Terminate the frame chain so a backtrace stops here. */
    xorq %rbp, %rbp

    /* Align the stack to 16 bytes. */
    andq $-16, %rsp

    /*
     * Allocate a small synthetic argument area:
     *    [rsp+0]  = NULL        (envp terminator)
     *    [rsp+8]  = NULL        (argv[1] terminator = end of argv)
     *    [rsp+16] = empty string, the buffer argv[0] points at
     */
    subq $32, %rsp
    movq $0, 0(%rsp)
    movq $0, 8(%rsp)
    movb $0, 16(%rsp)

    /* argv = &argv[0] = pointer to the slot holding &""
     * We need argv[0] to point at the empty string, so reserve one more slot. */
    subq $16, %rsp
    leaq 32(%rsp), %rax          /* points at the empty string buffer */
    movq %rax, 0(%rsp)           /* argv[0] = ""                    */
    movq $0, 8(%rsp)             /* argv[1] = NULL                  */

    leaq 0(%rsp), %rsi           /* argv */
    leaq 16(%rsp), %rdx          /* envp = the NULL slot above      */
    movl $1, %edi                /* argc = 1 (argv[0] only)         */

    call main

    /* main returned; exit(status) via SYS_EXIT. */
    movl %eax, %edi
    movq $4, %rax                /* SYS_EXIT */
    syscall

    /* SYS_EXIT does not return. If it somehow does, stop the CPU cleanly. */
1:  hlt
    jmp 1b

    .size _start, . - _start

    /* Mark the stack non-executable. Without this the linker assumes an
     * executable stack for the whole image, which would silently undo NX. */
    .section .note.GNU-stack, "", @progbits
