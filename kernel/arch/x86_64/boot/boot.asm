; NEXUS OS - Bootable Kernel Entry Point
; arch/x86_64/boot/boot.asm
; NASM assembly - 64-bit kernel entry point

[bits 64]
[default rel]

section .text
    align 8

    ; Multiboot2 header - must be in first 8KB of file
    MBOOT2_MAGIC equ 0xE85250D6
    MBOOT2_ARCH equ 0x00000000  ; Protected mode (i386)
    MBOOT2_HEADER_LEN equ mboot2_header_end - mboot2_header
    MBOOT2_CHECKSUM equ -(MBOOT2_MAGIC + MBOOT2_ARCH + MBOOT2_HEADER_LEN)

    ; Entry point
    global _start
    global _start_kernel
    extern kernel_main
    extern kernel_panic

_start:
    ; Multiboot2 header immediately after entry point
    mboot2_header:
        dd MBOOT2_MAGIC           ; Magic number
        dw MBOOT2_ARCH            ; Architecture (0 = i386)
        dw MBOOT2_HEADER_LEN      ; Header length
        dd MBOOT2_CHECKSUM        ; Checksum
        ; End tag
        dw 0                      ; Type (0 = end)
        dw 0                      ; Flags
        dd 8                      ; Size
    mboot2_header_end:

    ; Continue with kernel entry
    mov rsp, stack_top
    push 0
    popfq
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000000
    jbe .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt

.no_long_mode:
    mov rdi, .error_msg
    call kernel_panic
    
.error_msg:
    db "Error: x86_64 long mode not supported!", 0

global inb
inb:
    xor eax, eax
    mov dx, di
    in al, dx
    ret

global outb
outb:
    mov dx, di
    mov eax, esi
    out dx, al
    ret

global inw
inw:
    xor eax, eax
    mov dx, di
    in ax, dx
    ret

global outw
outw:
    mov dx, di
    mov eax, esi
    out dx, ax
    ret

global inl
inl:
    xor eax, eax
    mov dx, di
    in eax, dx
    ret

global outl
outl:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

global io_wait
io_wait:
    out 0x80, al
    ret

global cpuid
cpuid:
    push rbx
    mov eax, edi
    mov ecx, esi
    cpuid
    pop rbx
    ret

global get_cpu_id
get_cpu_id:
    mov eax, 1
    cpuid
    mov eax, ebx
    shr eax, 24
    ret

global halt_cpu
halt_cpu:
    cli
    hlt
    ret

global arch_halt_cpu
arch_halt_cpu:
    cli
    hlt
    ret

global enable_interrupts
enable_interrupts:
    sti
    ret

global arch_enable_interrupts
arch_enable_interrupts:
    sti
    ret

global disable_interrupts
disable_interrupts:
    cli
    ret

global arch_disable_interrupts
arch_disable_interrupts:
    cli
    ret

global arch_reboot
arch_reboot:
    ; Triple fault to reboot
    idt_ptr equ 0
    lidt [idt_ptr]
    int 3
    ret

global arch_shutdown
arch_shutdown:
    ; ACPI shutdown - for now just halt
    cli
    hlt
    ret

global arch_get_cpu_id
arch_get_cpu_id:
    mov eax, 1
    cpuid
    mov eax, ebx
    shr eax, 24
    ret

global arch_get_ticks
arch_get_ticks:
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret

global arch_early_init
arch_early_init:
    ret

global arch_late_init
arch_late_init:
    ret

global get_rflags
get_rflags:
    pushfq
    pop rax
    ret

global read_cr0
read_cr0:
    mov rax, cr0
    ret

global read_cr3
read_cr3:
    mov rax, cr3
    ret

global read_cr4
read_cr4:
    mov rax, cr4
    ret

global write_cr3
write_cr3:
    mov cr3, rdi
    ret

global read_msr
read_msr:
    mov ecx, edi
    rdmsr
    shl rdx, 32
    or rax, rdx
    ret

global write_msr
write_msr:
    mov ecx, edi
    mov eax, esi
    mov edx, esi
    shr rdx, 32
    wrmsr
    ret

global read_tsc
read_tsc:
    rdtsc
    shl rdx, 32
    or rax, rdx
    ret

global read_tsc_aux
read_tsc_aux:
    rdtscp
    shl rdx, 32
    or rax, rdx
    ret

global memory_barrier
memory_barrier:
    mfence
    ret

global read_memory_barrier
read_memory_barrier:
    lfence
    ret

global write_memory_barrier
write_memory_barrier:
    sfence
    ret

global cpu_pause
cpu_pause:
    pause
    ret

global syscall_invoke
syscall_invoke:
    mov rax, rdi
    mov rdi, rsi
    mov rsi, rdx
    mov rdx, r10
    mov r10, r8
    mov r8, r9
    syscall
    ret

global get_stack_pointer
get_stack_pointer:
    mov rax, rsp
    ret

global get_base_pointer
get_base_pointer:
    mov rax, rbp
    ret

global get_instruction_pointer
get_instruction_pointer:
    mov rax, [rsp]
    ret

global early_memcpy
early_memcpy:
    push rdi
    push rsi
    push rdx
    mov rcx, rdx
    rep movsb
    pop rdx
    pop rsi
    pop rdi
    mov rax, rdi
    ret

global early_memset
early_memset:
    push rdi
    push rsi
    push rdx
    mov rax, rdi
    mov rcx, rdx
    mov al, sil
    rep stosb
    pop rdx
    pop rsi
    pop rdi
    mov rax, rdi
    ret

global _start_kernel
_start_kernel:
    jmp _start

section .data
    align 8

global gdt_descriptor
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start

align 16
gdt_start:
    dq 0x0000000000000000
    
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 00100000b
    db 0x00
    dd 0x00000000
    
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 00000000b
    db 0x00
    dd 0x00000000
    
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11111010b
    db 00100000b
    db 0x00
    dd 0x00000000
    
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11110010b
    db 00000000b
    db 0x00
    dd 0x00000000
gdt_end:

align 16
global tss_start
tss_start:
    dd 0x00000000
    dq stack_top
    dq 0x00000000
    dq 0x00000000
    dq 0x00000000
    dq 0x00000000
    dq 0x00000000
    dq 0x00000000
    dq 0x00000000
    dw 0x0000
    dw 0x68
tss_end:

global tss_descriptor
tss_descriptor:
    dw tss_end - tss_start - 1
    dw 0
    db 0
    db 10001001b
    db 0x40
    db 0
    dq 0
    dd 0

section .bss
    align 16
    stack_bottom:
        resb 65536
    stack_top:
